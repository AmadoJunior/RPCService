#pragma once
#include "HTTPServer.h"
#include "BumpMemoryManager.h"
#include <memory>
#include <memory_resource>
#include <string>

    class ServerManager {
    private:
        std::shared_ptr<BumpMemoryManager> memoryManager_;
        std::pmr::memory_resource* resource_;
        std::unique_ptr<HTTPServer> server_;

        HTTPServer::Response createHelloWorldResponse(std::pmr::memory_resource* resource);

    public:
        // Constructor
        explicit ServerManager(std::shared_ptr<BumpMemoryManager> memoryManager);

        // Destructor
        ~ServerManager();

        // Register all routes with the server
        void setupRoutes();

        // Start the server on the specified address and port
        bool start(const std::pmr::string& address, uint16_t port);

        // Stop the server
        void stop();

        // Deleted copy and move operations
        ServerManager(const ServerManager&) = delete;
        ServerManager& operator=(const ServerManager&) = delete;
        ServerManager(ServerManager&&) = delete;
        ServerManager& operator=(ServerManager&&) = delete;
    };