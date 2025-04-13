#include "ServerManager.h"
#include "WinsockSocket.h"
#include "PMRDeleter.h"
#include <iostream>
#include <format>

ServerManager::ServerManager(std::shared_ptr<BumpMemoryManager> memoryManager)
    : memoryManager_(std::move(memoryManager)),
    resource_(memoryManager_->getResource()),
    server_(nullptr, PMRDeleter<HTTPServer>(resource_))
{
    // Create Socket
    auto socketImpl = make_pmr_unique_ptr<WinsockSocket>(resource_, resource_);

    // Create a unique_ptr<Socket> from unique_ptr<WinsockSocket>
    std::unique_ptr<Socket, PMRDeleter<Socket>> socket(
        socketImpl.release(),
        PMRDeleter<Socket>(resource_)
    );


    // Create Server
    server_ = make_pmr_unique_ptr<HTTPServer>(
        resource_,
        std::move(socket),
        memoryManager_
    );
}

ServerManager::~ServerManager() {
    stop();
}

void ServerManager::setupRoutes() {
    // Setup GET Root Path
    std::pmr::vector<std::pmr::string> methodsAllowed(resource_);
    methodsAllowed.push_back(std::pmr::string("GET", resource_));

    server_->registerHandlerWithMethods(
        std::pmr::string("/", resource_),
        methodsAllowed,
        [this](const HTTPServer::Request& req) -> HTTPServer::Response {
            return createHelloWorldResponse(req.method.get_allocator().resource());
        });

    // Methods
    std::pmr::vector<std::pmr::string> apiMethods(resource_);
    apiMethods.push_back(std::pmr::string("GET", resource_));

    // Handler
    server_->registerHandlerWithMethods(
        std::pmr::string("/api/data", resource_),
        apiMethods,
        [this](const HTTPServer::Request& req) -> HTTPServer::Response {
            auto* reqResource = req.method.get_allocator().resource();
            HTTPServer::Response res(200, {}, reqResource);

            if (req.method == "GET") {
                std::pmr::string body("{\"status\": \"success\", \"message\": \"Data retrieved successfully\"}", reqResource);
                res.body = std::pmr::vector<uint8_t>(body.begin(), body.end(), reqResource);
            }
            else if (req.method == "POST") {
                std::pmr::string body("{\"status\": \"success\", \"message\": \"Data saved successfully\"}", reqResource);
                res.body = std::pmr::vector<uint8_t>(body.begin(), body.end(), reqResource);
            }

            res.headers[std::pmr::string("Content-Type", reqResource)] =
                std::pmr::string("application/json", reqResource);
            return res;
        });
}

HTTPServer::Response ServerManager::createHelloWorldResponse(std::pmr::memory_resource* resource) {
    HTTPServer::Response res(200, {}, resource);
    std::pmr::string body("Hello, World!", resource);
    res.body = std::pmr::vector<uint8_t>(body.begin(), body.end(), resource);
    res.headers[std::pmr::string("Content-Type", resource)] =
        std::pmr::string("text/plain", resource);
    return res;
}

bool ServerManager::start(const std::pmr::string& address, uint16_t port) {
    auto startResult = server_->start(address, port);
    if (startResult.type != SocketError::Type::None) {
        std::cerr << "Failed to start server: " << static_cast<int>(startResult.type)
            << " Internal error code: " << startResult.internalCode << std::endl;
        return false;
    }

    std::cout << std::format("Server running: {}:{}",
        std::string(address.begin(), address.end()),
        port) << std::endl;
    return true;
}

void ServerManager::stop() {
    if (server_) {
        server_->stop();
    }
}