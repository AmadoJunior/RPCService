// HTTPServer.cpp
#include "HTTPServer.h"

HTTPServer::HTTPServer(std::unique_ptr<Socket> socket)
    : socket_(std::move(socket)), running_(false) {
}

HTTPServer::~HTTPServer() {
    stop();
}

SocketError HTTPServer::start(const std::string& address, uint16_t port) {
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

    cleanupThread_ = std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            cleanupSessions();
        }
    });

    acceptThread_ = std::thread([this]() {
        while (running_) {
            auto clientResult = socket_->accept();
            if (!clientResult.has_value()) {
                if (running_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                continue;
            }

            auto clientSocket = clientResult.value();

            if (clientSocket->setTimeout()) continue;

            std::lock_guard<std::mutex> lock(clientSessionsMutex_);
            try {
                auto session = std::make_unique<ClientSession>(clientSocket);
                session->setThread(std::thread(&HTTPServer::handleClient, this, clientSocket, std::ref(*session)));
                clientSessions_.push_back(std::move(session));
            }
            catch (const std::exception&) {
                // Log Errors
                std::cerr << "acceptThread_ Error" << std::endl;
            }
        }
    });


    return SocketError::success();
}

void HTTPServer::cleanupSessions() {
    std::lock_guard<std::mutex> lock(clientSessionsMutex_);

    auto it = clientSessions_.begin();
    while (it != clientSessions_.end()) {
        auto& session = *it;
        if (!session->active) {
            it = clientSessions_.erase(it);
        }
        else {
            ++it;
        }
    }
}


void HTTPServer::stop() {
    if (!running_) return;

    running_ = false;

    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    // Final Cleanup of All Sessions
    cleanupSessions();

    socket_->close();
    socket_->cleanup();
}

//void HTTPServer::registerHandler(const std::string& path, RequestHandler handler) {
//    handlers_[path] = std::move(handler);
//}

HTTPServer::Request HTTPServer::parseRequest(const std::vector<uint8_t>& data) {
    Request request;
    std::string rawRequest(data.begin(), data.end());
    std::istringstream stream(rawRequest);
    std::string line;

    // Parse Request Line
    if (std::getline(stream, line)) {
        // Remove \r If Present at The End
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::istringstream lineStream(line);
        lineStream >> request.method >> request.path;
    }


    // Parse Headers
    while (std::getline(stream, line)) {
        // Remove \r If Present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Empty Line Signals End of Headers
        if (line.empty()) {
            break;
        }

        auto colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            // Skip Colon and Leading Spaces
            size_t valueStart = line.find_first_not_of(" \t", colonPos + 1);
            std::string value = (valueStart != std::string::npos) ?
                line.substr(valueStart) : "";
            request.headers[key] = value;
        }
    }


    // Read Body if Content-Length is Present
    auto contentLength = request.headers.find("Content-Length");
    if (contentLength != request.headers.end()) {
        size_t length = std::stoul(contentLength->second);
        std::vector<uint8_t> body(length);
        stream.read(reinterpret_cast<char*>(body.data()), length);
        request.body = std::move(body);
    }

    return request;
}

std::vector<uint8_t> HTTPServer::serializeResponse(const Response& response) {
    std::ostringstream stream;

    // Status Line
    stream << "HTTP/1.1 " << response.statusCode << " ";
    switch (response.statusCode) {
    case 200: stream << "OK"; break;
    case 404: stream << "Not Found"; break;
    default: stream << "Unknown";
    }
    stream << "\r\n";

    // Headers
    for (const auto& [key, value] : response.headers) {
        stream << key << ": " << value << "\r\n";
    }
    stream << "Content-Length: " << response.body.size() << "\r\n";
    stream << "\r\n";

    // Convert Headers to Vector
    std::string headerStr = stream.str();
    std::vector<uint8_t> responseData(headerStr.begin(), headerStr.end());

    // Append Body
    responseData.insert(responseData.end(), response.body.begin(), response.body.end());

    return responseData;
}

void HTTPServer::handleClient(std::shared_ptr<Socket> clientSocket, ClientSession& session) {
    while (running_) {
        try {
            // Attempt to Receive Data
            auto receiveResult = clientSocket->receive(16384);

            // No Data Available
            if (!receiveResult.has_value()) {
                // Sleep to Prevent Tight Polling
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            //Connection Closed
            if (receiveResult.value().empty()) break;

            // Update Last Activity Timestamp
            session.lastActivity = std::chrono::steady_clock::now();

            // Parse and Process the Request
            auto request = parseRequest(receiveResult.value());

            Response response{ 405, {}, {} };

            auto matchingRoute = findMatchingRoute(request.path, request.method);
            if (matchingRoute) {
                try {
                    response = matchingRoute->handler(request);
                }
                catch (const std::exception& e) {
                    response.statusCode = 500;
                    std::string error = "Internal Server Error";
                    response.body = std::vector<uint8_t>(error.begin(), error.end());
                }
            }
            else {
                bool pathExists = false;
                std::string allowedMethods;

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
                    response.headers["Allow"] = allowedMethods;
                }
                else {
                    // Not Found
                    response.statusCode = 404; 
                }
            }

            auto handlerIt = handlers_.find(request.path);
            if (handlerIt != handlers_.end()) {
                try {
                    response = handlerIt->second(request);
                }
                catch (const std::exception& e) {
                    response.statusCode = 500;
                    std::string error = "Internal Server Error";
                    response.body = std::vector<uint8_t>(error.begin(), error.end());
                }
            }

            // Connection Handling
            bool keepAlive = true;
            auto connectionHeader = request.headers.find("Connection");
            if (connectionHeader != request.headers.end()) {
                std::string connectionValue = connectionHeader->second;
                std::transform(connectionValue.begin(), connectionValue.end(), connectionValue.begin(), ::tolower);
                keepAlive = (connectionValue != "close");
            }

            // Set Connection Headers
            response.headers["Connection"] = keepAlive ? "keep-alive" : "close";
            if (keepAlive) {
                response.headers["Keep-Alive"] = "timeout=60, max=100";
            }

            // Serialize and Send Response
            auto responseData = serializeResponse(response);
            auto sendResult = clientSocket->send(responseData);

            // Keeping Alive or Send Fails
            if (sendResult.type != SocketError::Type::None || !keepAlive) {
                break;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Client Handler Exception: " << e.what() << std::endl;
            break;
        }
        catch (...) {
            std::cerr << "Unknown Exception in Client Handler" << std::endl;
            break;
        }
    }

    // Ensure Socket Closed
    session.active = false;
}

void HTTPServer::registerHandler(const std::string& path, RequestHandler handler) {
    registerHandlerWithMethods(path, std::vector<std::string>{}, handler);  // Empty vector means all methods allowed
}

void HTTPServer::registerHandlerWithMethods(const std::string& path, const std::vector<std::string>& methods, RequestHandler handler) {
    RouteConfig route{ path, methods, handler };
    routes_.push_back(std::move(route));
}

bool HTTPServer::isMethodAllowed(const std::vector<std::string>& allowedMethods, const std::string& method) const {
    if (allowedMethods.empty()) {
        return true;
    }
    return std::find(allowedMethods.begin(), allowedMethods.end(), method) != allowedMethods.end();
}

std::optional<HTTPServer::RouteConfig> HTTPServer::findMatchingRoute(const std::string& path, const std::string& method) {
    for (const auto& route : routes_) {
        if (route.path == path && isMethodAllowed(route.allowedMethods, method)) {
            return route;
        }
    }
    return std::nullopt;
}