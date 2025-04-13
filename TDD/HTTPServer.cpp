#include "HTTPServer.h"
#include <iostream>
#include <sstream>
#include <format>
#include <algorithm>

HTTPServer::HTTPServer(
    std::unique_ptr<Socket> socket,
    std::shared_ptr<BumpMemoryManager> memoryManager
) :
    socket_(std::move(socket)),
    memoryManager_(std::move(memoryManager)),
    serverResource_(memoryManager_->getResource()),
    running_(false),
    handlers_(serverResource_),
    routes_(serverResource_),
    clientSessions_(serverResource_),
    clientSessionBufferSize_(256 * 1024)  // 256KB per Client by Default
{
}

HTTPServer::~HTTPServer() {
    stop();
}

SocketError HTTPServer::start(const std::pmr::string& address, uint16_t port) {
    auto initResult = socket_->init();
    if (initResult.type != SocketError::Type::None) {
        return initResult;
    }

    auto bindResult = socket_->bind(address, port);
    if (bindResult.type != SocketError::Type::None) {
        return bindResult;
    }

    auto listenResult = socket_->listen(100);
    if (listenResult.type != SocketError::Type::None) {
        return listenResult;
    }

    running_ = true;

    // Start Cleanup Thread
    cleanupThread_ = std::thread(&HTTPServer::cleanupThreadHandler, this);

    // Start accept thread
    acceptThread_ = std::thread(&HTTPServer::acceptThreadHandler, this);

    return SocketError::success();
}

void HTTPServer::stop() {
    if (!running_) return;

    running_ = false;

    // Wait Threads
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    // Final Cleanup of Sessions
    cleanupSessions();

    // Close and Cleanup the Socket
    socket_->close();
    socket_->cleanup();
}

void HTTPServer::cleanupSessions() {
    std::lock_guard<std::mutex> lock(clientSessionsMutex_);

    auto it = clientSessions_.begin();
    while (it != clientSessions_.end()) {
        auto& session = *it;
        if (!session->isActive()) {
            it = clientSessions_.erase(it);
        }
        else {
            ++it;
        }
    }
}

void HTTPServer::registerHandler(const std::pmr::string& path, RequestHandler handler) {
    registerHandlerWithMethods(path, std::pmr::vector<std::pmr::string>(serverResource_), handler);
}

void HTTPServer::registerHandlerWithMethods(
    const std::pmr::string& path,
    const std::pmr::vector<std::pmr::string>& methods,
    RequestHandler handler
) {
    RouteConfig route(serverResource_);
    route.path = path;
    route.allowedMethods = methods;
    route.handler = std::move(handler);
    routes_.push_back(std::move(route));
}

HTTPServer::Request HTTPServer::parseRequest(
    const std::pmr::vector<uint8_t>& data,
    std::pmr::memory_resource* resource
) {
    Request request(resource);
    std::pmr::string rawRequest(data.begin(), data.end(), resource);

    std::istringstream stream(std::string(rawRequest.begin(), rawRequest.end()));
    std::pmr::string line(resource);

    // Parse Request Line
    if (std::getline(stream, line)) {
        // Remove \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream lineStream(line);
        std::string method, path;
        lineStream >> method >> path;

        request.method = std::pmr::string(method, resource);
        request.path = std::pmr::string(path, resource);
    }

    // Parse Headers
    while (std::getline(stream, line)) {
        // Remove \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Empty Line Signals End of Headers
        if (line.empty()) {
            break;
        }

        auto colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::pmr::string key = line.substr(0, colonPos);
            // Skip Colon and Leading Spaces
            size_t valueStart = line.find_first_not_of(" \t", colonPos + 1);
            std::pmr::string value = (valueStart != std::pmr::string::npos) ?
                line.substr(valueStart) : "";

            request.headers[std::pmr::string(key, resource)] =
                std::pmr::string(value, resource);
        }
    }

    // Read Body if Content-Length Present
    auto contentLength = request.headers.find(std::pmr::string("Content-Length", resource));
    if (contentLength != request.headers.end()) {
        size_t length = std::stoul(contentLength->second.c_str());
        request.body.resize(length, 0);

        stream.read(reinterpret_cast<char*>(request.body.data()), length);
    }

    return request;
}

std::pmr::vector<uint8_t> HTTPServer::serializeResponse(
    const Response& response,
    std::pmr::memory_resource* resource
) {
    std::pmr::string headerString(resource);

    // Status Line
    headerString += "HTTP/1.1 " + std::to_string(response.statusCode) + " ";
    switch (response.statusCode) {
    case 200: headerString += "OK"; break;
    case 400: headerString += "Bad Request"; break;
    case 404: headerString += "Not Found"; break;
    case 405: headerString += "Method Not Allowed"; break;
    case 500: headerString += "Internal Server Error"; break;
    default: headerString += "Unknown";
    }
    headerString += "\r\n";

    // Headers
    for (const auto& [key, value] : response.headers) {
        headerString += std::pmr::string(key.begin(), key.end(), resource);
        headerString += ": ";
        headerString += std::pmr::string(value.begin(), value.end(), resource);
        headerString += "\r\n";
    }

    // Content-Length Header
    headerString += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";

    // Empty Line to Separate HJeaders from Body
    headerString += "\r\n";

    // Create Response Data with Headers
    std::pmr::vector<uint8_t> responseData(headerString.begin(), headerString.end(), resource);

    // Append Body
    responseData.insert(responseData.end(), response.body.begin(), response.body.end());

    return responseData;
}

bool HTTPServer::isMethodAllowed(
    const std::pmr::vector<std::pmr::string>& allowedMethods,
    const std::pmr::string& method
) const {
    if (allowedMethods.empty()) {
        return true;  // All Methods Allowed
    }

    return std::find(allowedMethods.begin(), allowedMethods.end(), method) != allowedMethods.end();
}

std::optional<HTTPServer::RouteConfig> HTTPServer::findMatchingRoute(
    const std::pmr::string& path,
    const std::pmr::string& method
) {
    for (const auto& route : routes_) {
        if (route.path == path && isMethodAllowed(route.allowedMethods, method)) {
            return route;
        }
    }
    return std::nullopt;
}

void HTTPServer::handleClient(ClientSession& session) {
    auto* sessionResource = session.getResource();
    auto clientSocket = session.getSocket();

    while (running_ && session.isActive()) {
        try {
            // Attempt to Receive Data
            auto receiveResult = clientSocket->receive(16384);

            // No Data Available
            if (!receiveResult.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Connection Closed
            if (receiveResult.value().empty()) {
                break;
            }

            // Update TS
            session.updateLastActivityTime();

            // Parse and Process Request
            auto request = parseRequest(receiveResult.value(), sessionResource);

            // Prepare Default 405 Response
            Response response(405, {}, sessionResource);

            // Find Matching Route
            auto matchingRoute = findMatchingRoute(request.path, request.method);
            if (matchingRoute) {
                try {
                    response = matchingRoute->handler(request);
                }
                catch (const std::exception& e) {
                    response.statusCode = 500;
                    std::string error = "Internal Server Error: ";
                    error += e.what();
                    std::pmr::string pmrError(error, sessionResource);
                    response.body = std::pmr::vector<uint8_t>(
                        pmrError.begin(), pmrError.end(), sessionResource
                    );
                }
            }
            else {
                bool pathExists = false;
                std::pmr::string allowedMethods(sessionResource);

                for (const auto& route : routes_) {
                    if (route.path == request.path) {
                        pathExists = true;
                        for (const auto& method : route.allowedMethods) {
                            if (!allowedMethods.empty()) allowedMethods += ", ";
                            allowedMethods += method;
                        }
                        break;
                    }
                }

                if (pathExists) {
                    // Method Not Allowed
                    response.statusCode = 405;
                    response.headers[std::pmr::string("Allow", sessionResource)] = allowedMethods;
                }
                else {
                    // Not Found
                    response.statusCode = 404;
                    std::pmr::string notFoundMsg("Resource Not Found", sessionResource);
                    response.body = std::pmr::vector<uint8_t>(
                        notFoundMsg.begin(), notFoundMsg.end(), sessionResource
                    );
                }
            }

            // Check for Handlers in Legacy Map (Compatibility)
            auto handlerIt = handlers_.find(request.path);
            if (handlerIt != handlers_.end()) {
                try {
                    response = handlerIt->second(request);
                }
                catch (const std::exception& e) {
                    response.statusCode = 500;
                    std::string error = "Internal Server Error: ";
                    error += e.what();
                    std::pmr::string pmrError(error, sessionResource);
                    response.body = std::pmr::vector<uint8_t>(
                        pmrError.begin(), pmrError.end(), sessionResource
                    );
                }
            }

            // Connection Handling
            bool keepAlive = true;
            auto connectionHeader = request.headers.find(std::pmr::string("Connection", sessionResource));
            if (connectionHeader != request.headers.end()) {
                std::pmr::string connectionValue = connectionHeader->second;
                std::transform(connectionValue.begin(), connectionValue.end(),
                    connectionValue.begin(), ::tolower);
                keepAlive = (connectionValue != "close");
            }

            // Set Connection Headers
            if (keepAlive) {
                response.headers[std::pmr::string("Connection", sessionResource)] =
                    std::pmr::string("keep-alive", sessionResource);
                response.headers[std::pmr::string("Keep-Alive", sessionResource)] =
                    std::pmr::string("timeout=60, max=100", sessionResource);
            }
            else {
                response.headers[std::pmr::string("Connection", sessionResource)] =
                    std::pmr::string("close", sessionResource);
            }

            // Serialize and Send Response
            auto responseData = serializeResponse(response, sessionResource);
            auto sendResult = clientSocket->send(responseData);

            if (sendResult.type != SocketError::Type::None || !keepAlive) {
                break;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Client handler exception: " << e.what() << std::endl;
            break;
        }
        catch (...) {
            std::cerr << "Unknown exception in client handler" << std::endl;
            break;
        }
    }

    session.markInactive();
}

void HTTPServer::acceptThreadHandler() {
    while (running_) {
        auto clientResult = socket_->accept();
        if (!clientResult.has_value()) {
            if (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            continue;
        }

        auto clientSocket = clientResult.value();

        std::lock_guard<std::mutex> lock(clientSessionsMutex_);
        try {
            auto clientResource = memoryManager_->createClientResource(clientSessionBufferSize_);

            auto session = std::make_unique<ClientSession>(
                clientSocket,
                std::move(clientResource),
                [this](ClientSession& session) { this->handleClient(session); }
            );

            clientSessions_.push_back(std::move(session));
            clientSessions_.back()->start();
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to create client session: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Unknown error creating client session" << std::endl;
        }
    }
}

void HTTPServer::cleanupThreadHandler() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        cleanupSessions();
    }
}