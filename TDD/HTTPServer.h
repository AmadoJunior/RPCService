// HTTPServer.h
#pragma once

#ifdef LIB_EXPORTS
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif

#include "Socket.h"
#include <memory>
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

class API ClientSession {
public:
    ClientSession(std::shared_ptr<Socket> socket)
        : socket(std::move(socket)), active(true), lastActivity(std::chrono::steady_clock::now()) {
    }

    ~ClientSession() {
        if (socket) {
            socket->close();
        }
        if (thread.joinable()) {
            thread.join();
        }
    }

    void setThread(std::thread t) {
        thread = std::move(t);
    }

    std::thread thread;
    std::shared_ptr<Socket> socket;
    bool active;
    std::chrono::steady_clock::time_point lastActivity;
};

class API HTTPServer {
public:
    struct Request {
        std::string method;
        std::string path;
        std::unordered_map<std::string, std::string> headers;
        std::vector<uint8_t> body;
    };

    struct Response {
        int statusCode;
        std::unordered_map<std::string, std::string> headers;
        std::vector<uint8_t> body;
    };

    using RequestHandler = std::function<Response(const Request&)>;

    struct RouteConfig {
        std::string path;
        std::vector<std::string> allowedMethods;
        RequestHandler handler;
    };

    HTTPServer(std::unique_ptr<Socket> socket);
    ~HTTPServer();

    SocketError start(const std::string& address, uint16_t port);
    void stop();

    void registerHandler(const std::string& path, RequestHandler handler);
    void registerHandlerWithMethods(const std::string& path, const std::vector<std::string>& methods, RequestHandler handler);
    void cleanupSessions();
    Request parseRequest(const std::vector<uint8_t>& data);
    std::vector<uint8_t> serializeResponse(const Response& response);
    void handleClient(std::shared_ptr<Socket> clientSocket, ClientSession& session);
private:
    std::atomic<bool> running_;
    std::unique_ptr<Socket> socket_;
    std::unordered_map<std::string, RequestHandler> handlers_;
    std::vector<RouteConfig> routes_;
    std::thread acceptThread_;
    std::thread cleanupThread_;
    std::vector<std::unique_ptr<ClientSession>> clientSessions_;
    std::mutex clientSessionsMutex_;

    std::optional<RouteConfig> findMatchingRoute(const std::string& path, const std::string& method);
    bool isMethodAllowed(const std::vector<std::string>& allowedMethods, const std::string& method) const;
};