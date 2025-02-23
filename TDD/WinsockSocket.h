#pragma once

#ifdef LIB_EXPORTS
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif

#include "Socket.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <expected>

class API WinsockSocket : public Socket {
private:
    SOCKET sock_;
    bool initialized_;
    static bool winsockInitialized_;

    static SocketError getLastError(SocketError::Type type);
    static SocketError initWinsock();

public:
    WinsockSocket();
    ~WinsockSocket() override;

    SocketError init() override;
    void cleanup() override;
    SocketError bind(const std::string& address, uint16_t port) override;
    SocketError listen(int backlog) override;
    std::expected<std::shared_ptr<Socket>, SocketError> accept() override;
    SocketError connect(const std::string& address, uint16_t port) override;
    SocketError send(const std::vector<uint8_t>& data) override;
    std::expected<std::vector<uint8_t>, SocketError> receive(size_t maxSize) override;
    bool isSameSocket(const std::shared_ptr<Socket>& other) const override;
    void close() override;
    int setTimeout() override;
};