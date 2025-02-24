#include "Calculator.h"
#include <iostream>
#include <winsock2.h>
#include "HTTPServer.h"
#include "WinsockSocket.h"
#include <csignal>
#include <chrono>
#include <atomic>
#include <format>

std::atomic<bool> running = true;

void signalHandler(int signum) {
    running = false;
}

int main() {
    signal(SIGINT, signalHandler);  // Handle Ctrl+C
    signal(SIGTERM, signalHandler); // Handle Termination Request

    HTTPServer server(std::make_unique<WinsockSocket>());
    std::vector<std::string> methodsAllowed = { "GET" };
    server.registerHandlerWithMethods("/", methodsAllowed, [](const HTTPServer::Request& req) {
        HTTPServer::Response res;
        res.statusCode = 200;
        std::string body = "Hello, World!";
        res.body = std::vector<uint8_t>(body.begin(), body.end());
        return res;
    });

    std::string serverAddress = "127.0.0.1";
    const int serverPort = 8080;
    auto startResult = server.start(serverAddress, serverPort);
    if (startResult.type != SocketError::Type::None) {
        std::cerr << "Failed to Start Server: " << static_cast<int>(startResult.type)
            << " Internal Error Code: " << startResult.internalCode << std::endl;
        return 1;
    }

    std::cout << std::format("Server Running: {}:{}", serverAddress, serverPort) << std::endl;

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.stop();
    return 0;
}