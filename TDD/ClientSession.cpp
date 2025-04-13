#include "ClientSession.h"
#include <iostream>

ClientSession::ClientSession(
    std::shared_ptr<Socket> socket,
    std::unique_ptr<std::pmr::memory_resource, BumpMemoryManager::CustomDeleter> resource,
    ClientHandlerFunc handler
) :
    socket_(std::move(socket)),
    resource_(std::move(resource)),
    active_(true),
    lastActivity_(std::chrono::steady_clock::now()),
    handlerFunc_(std::move(handler))
{
}

ClientSession::~ClientSession() {
    markInactive();

    if (thread_.joinable()) {
        thread_.join();
    }

    if (socket_) {
        socket_->close();
    }
}

void ClientSession::start() {
    thread_ = std::thread(&ClientSession::threadHandler, this);
}

std::pmr::memory_resource* ClientSession::getResource() const {
    return resource_.get();
}

std::shared_ptr<Socket> ClientSession::getSocket() const {
    return socket_;
}

bool ClientSession::isActive() const {
    return active_.load();
}

void ClientSession::markInactive() {
    active_.store(false);
}

std::chrono::steady_clock::time_point ClientSession::getLastActivityTime() const {
    return lastActivity_;
}

void ClientSession::updateLastActivityTime() {
    lastActivity_ = std::chrono::steady_clock::now();
}

void ClientSession::threadHandler() {
    try {
        if(isActive()) handlerFunc_(*this);
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in client thread: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown exception in client thread" << std::endl;
    }

    markInactive();
}