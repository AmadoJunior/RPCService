// HTTPServer.h
#pragma once

#ifdef LIB_EXPORTS
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif

#include "Socket.h"
#include "ClientSession.h"
#include "BumpMemoryManager.h"
#include "PMRDeleter.h"
#include <memory>
#include <memory_resource> 
#include <string>
#include <functional>
#include <unordered_map>
#include <thread>
#include <sstream>
#include <mutex>
#include <format>
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <algorithm>

class API HTTPServer {
public:
    struct Request {
        std::pmr::string method;
        std::pmr::string path;
        std::pmr::string version;
        std::pmr::unordered_map<std::pmr::string, std::pmr::string> headers;
        std::pmr::vector<uint8_t> body;

        Request(std::pmr::memory_resource* resource)
            : method(resource), path(resource),
            headers(resource), body(resource) {
        }
    };

    struct Response {
        int statusCode;
        std::pmr::unordered_map<std::pmr::string, std::pmr::string> headers;
        std::pmr::vector<uint8_t> body;

        Response(int code = 200,
            std::pmr::unordered_map<std::pmr::string, std::pmr::string> headers = {},
            std::pmr::memory_resource* resource = std::pmr::get_default_resource())
            : statusCode(code), headers(resource), body(resource) {
        }
    };

    using RequestHandler = std::function<Response(const Request&)>;

    struct RouteConfig {
        std::pmr::string path;
        std::pmr::vector<std::pmr::string> allowedMethods;
        RequestHandler handler;

        RouteConfig(std::pmr::memory_resource* resource)
            : path(resource), allowedMethods(resource) {
        }
    };

    HTTPServer(
        std::unique_ptr<Socket, PMRDeleter<Socket>> socket,
        std::shared_ptr<BumpMemoryManager> memoryManager
    );
    ~HTTPServer();

    SocketError start(const std::pmr::string& address, uint16_t port);
    void stop();

    void registerHandler(const std::pmr::string& path, RequestHandler handler);
    void registerHandlerWithMethods(const std::pmr::string& path,
        const std::pmr::vector<std::pmr::string>& methods,
        RequestHandler handler);
    
private:
    void handleClient(ClientSession& session);
    void cleanupSessions();
    Request parseRequest(const std::pmr::vector<uint8_t>& data, std::pmr::memory_resource* resource);
    std::pmr::vector<uint8_t> serializeResponse(const Response& response, std::pmr::memory_resource* resource);
    std::optional<RouteConfig> findMatchingRoute(const std::pmr::string& path, const std::pmr::string& method);
    bool isMethodAllowed(const std::pmr::vector<std::pmr::string>& allowedMethods, const std::pmr::string& method) const;

    void acceptThreadHandler();
    void cleanupThreadHandler();

    // Server State
    std::atomic<bool> running_;
    std::unique_ptr<Socket, PMRDeleter<Socket>> socket_;

    // Memory Management
    std::shared_ptr<BumpMemoryManager> memoryManager_;
    std::pmr::memory_resource* serverResource_;

    // Route Config
    std::pmr::unordered_map<std::pmr::string, RequestHandler> handlers_;
    std::pmr::vector<RouteConfig> routes_;

    // Threading
    std::thread acceptThread_;
    std::thread cleanupThread_;

    // Client Session Management
    std::pmr::vector<std::unique_ptr<ClientSession, PMRDeleter<ClientSession>>> clientSessions_;
    std::mutex clientSessionsMutex_;

    // Configuration
    size_t clientSessionBufferSize_;
};