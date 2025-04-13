#pragma once

#include "Socket.h"
#include "BumpMemoryManager.h"
#include "PMRDeleter.h"
#include <memory>
#include <memory_resource>
#include <thread>
#include <chrono>
#include <atomic>
#include <functional>

class ClientSession {
public:
    using ClientHandlerFunc = std::function<void(ClientSession&)>;

    // Constructor
    ClientSession(
        std::shared_ptr<Socket> socket,
        std::unique_ptr<std::pmr::memory_resource, BumpMemoryManager::CustomDeleter> resource,
        ClientHandlerFunc handler
    );

    // Destructor
    ~ClientSession();

    // Start
    void start();

    // Get Session's Memory Resource
    std::pmr::memory_resource* getResource() const;

    // Get Socket
    std::shared_ptr<Socket> getSocket() const;

    // Activity Tracking
    bool isActive() const;
    void markInactive();
    std::chrono::steady_clock::time_point getLastActivityTime() const;
    void updateLastActivityTime();

    // Deleted Copy/Move Ops
    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;
    ClientSession(ClientSession&&) = delete;
    ClientSession& operator=(ClientSession&&) = delete;

private:
    // Thread Handler Method
    void threadHandler();

    std::shared_ptr<Socket> socket_;
    std::unique_ptr<std::pmr::memory_resource, BumpMemoryManager::CustomDeleter> resource_;
    std::atomic<bool> active_;
    std::thread thread_;
    std::chrono::steady_clock::time_point lastActivity_;
    ClientHandlerFunc handlerFunc_;
};