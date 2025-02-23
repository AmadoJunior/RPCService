#pragma once

#ifdef LIB_EXPORTS
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif

#include <expected>
#include <optional>
#include <vector>
#include <memory>
#include <string>

struct API SocketError {
    enum class Type {
        None,
        Initialization,
        Bind,
        Connection,
        Send,
        Receive
    };
    Type type;
    int32_t internalCode;

    static SocketError success() {
        return { Type::None, 0 };
    }

    bool operator==(const SocketError&) const = default;
};

class API Socket {
public:
    virtual SocketError init() = 0;
    virtual void cleanup() = 0;
    virtual SocketError bind(const std::string& address, uint16_t port) = 0;
    virtual SocketError listen(int backlog) = 0;
    virtual std::expected<std::shared_ptr<Socket>, SocketError> accept() = 0;
    virtual SocketError connect(const std::string& address, uint16_t port) = 0;
    virtual SocketError send(const std::vector<uint8_t>& data) = 0;
    virtual std::expected<std::vector<uint8_t>, SocketError> receive(size_t maxSize) = 0;
    virtual void close() = 0;
    virtual int setTimeout() = 0;
    virtual ~Socket() = default;
    virtual bool isSameSocket(const std::shared_ptr<Socket>& other) const = 0;
};