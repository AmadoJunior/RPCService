#pragma once

#ifdef LIB_EXPORTS
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif

#include "Socket.h"
#include <winsock2.h>
#include <memory>
#include <vector>
#include <string>
#include <expected>
#include <memory_resource>

class API WinsockSocket : public Socket {
public:
    WinsockSocket(std::pmr::memory_resource* resource = std::pmr::get_default_resource());
    ~WinsockSocket() override;

    SocketError init() override;
    void cleanup() override;

    SocketError bind(const std::pmr::string& address, uint16_t port);

    SocketError listen(int backlog) override;

    std::expected<std::shared_ptr<Socket>, SocketError> accept() override;

    SocketError connect(const std::pmr::string& address, uint16_t port);

    SocketError send(const std::pmr::vector<uint8_t>& data);

    std::expected<std::pmr::vector<uint8_t>, SocketError> receive(size_t maxSize);

    void close() override;
    int setTimeout() override;
    bool isSameSocket(const std::shared_ptr<Socket>& other) const override;
    std::pmr::memory_resource* getMemoryResource() const override;
private:
    static SocketError getLastError(SocketError::Type type);
    static SocketError initWinsock();

    SOCKET sock_;
    bool initialized_;
    std::pmr::memory_resource* resource_;
    static bool winsockInitialized_;
};