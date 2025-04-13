#include "BumpMemoryManager.h"
#include "ServerManager.h"
#include "SignalHandler.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>

int main() {
    try {
        // Initialize Signal Handling
        SignalHandler::initialize();

        // Initialize Memory Management
        constexpr size_t BUFFER_SIZE = 1000 * 1024 * 1024; // 1000MB
        auto memoryManager = std::make_shared<BumpMemoryManager>(BUFFER_SIZE);

        // Get Server's Resource for Initial Allocations
        auto* resource = memoryManager->getResource();

        // Create and Configure
        ServerManager serverManager(memoryManager);
        serverManager.setupRoutes();

        // Start Server
        std::pmr::string serverAddress("127.0.0.1", resource);
        constexpr uint16_t serverPort = 8070;

        if (!serverManager.start(serverAddress, serverPort)) {
            return 1;
        }

        // Main Loop
        std::cout << "Server is Running. Press Ctrl+C to Stop." << std::endl;
        while (SignalHandler::isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::cout << "Shutting Down..." << std::endl;
        serverManager.stop();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown Error" << std::endl;
        return 1;
    }

    std::cout << "Server Shutdown Complete." << std::endl;
    return 0;
}