#include "WinsockSocket.h"
#include "Socket.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <expected>

SocketError WinsockSocket::getLastError(SocketError::Type type) {
    return { type, static_cast<int32_t>(WSAGetLastError()) };
}

SocketError WinsockSocket::initWinsock() {
    if (!winsockInitialized_) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return getLastError(SocketError::Type::Initialization);
        }
        winsockInitialized_ = true;
    }
    return SocketError::success();
}

WinsockSocket::WinsockSocket() : sock_(INVALID_SOCKET), initialized_(false) {}

WinsockSocket::~WinsockSocket() {
    close();
    if (winsockInitialized_) {
        winsockInitialized_ = false;
    }
}

void WinsockSocket::cleanup() {
    WSACleanup();
}

SocketError WinsockSocket::init() {
    if (initialized_) return SocketError::success();

    auto wsaResult = initWinsock();
    if (wsaResult.type != SocketError::Type::None) {
        return wsaResult;
    }

    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) {
        return getLastError(SocketError::Type::Initialization);
    }

    // Enable TCP keepalive
    //BOOL keepAlive = TRUE;
    //if (setsockopt(sock_, SOL_SOCKET, SO_KEEPALIVE,
    //    reinterpret_cast<const char*>(&keepAlive),
    //    sizeof(keepAlive)) == SOCKET_ERROR) {
    //    return getLastError(SocketError::Type::Initialization);
    //}

    //tcp_keepalive keepAliveParams{};
    //keepAliveParams.onoff = 1;                    // Enable Keepalive
    //keepAliveParams.keepalivetime = 30000;        // Start Sending After 30 Seconds of Idle
    //keepAliveParams.keepaliveinterval = 1000;     // Send every 1 second

    //DWORD bytesReturned;
    //if (WSAIoctl(sock_, SIO_KEEPALIVE_VALS, &keepAliveParams,
    //    sizeof(keepAliveParams), nullptr, 0,
    //    &bytesReturned, nullptr, nullptr) == SOCKET_ERROR) {
    //    return getLastError(SocketError::Type::Initialization);
    //}

    // Allow Socket Reuse
    BOOL opt = TRUE;
    if (setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
        return getLastError(SocketError::Type::Initialization);
    }
    // Add SO_EXCLUSIVEADDRUSE
    if (setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
        return getLastError(SocketError::Type::Initialization);
    }

    initialized_ = true;
    return SocketError::success();
}

SocketError WinsockSocket::bind(const std::string& address, uint16_t port) {
    if (!initialized_) {
        return { SocketError::Type::Initialization, 0 };
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        return getLastError(SocketError::Type::Bind);
    }

    if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        return getLastError(SocketError::Type::Bind);
    }

    return SocketError::success();
}

SocketError WinsockSocket::listen(int backlog) {
    if (!initialized_) {
        return { SocketError::Type::Initialization, 0 };
    }

    if (::listen(sock_, backlog) == SOCKET_ERROR) {
        return getLastError(SocketError::Type::Connection);
    }

    return SocketError::success();
}

std::expected<std::shared_ptr<Socket>, SocketError> WinsockSocket::accept() {
    if (!initialized_) {
        return std::unexpected(SocketError{ SocketError::Type::Initialization, 0 });
    }

    sockaddr_in clientAddr{};
    int clientAddrLen = sizeof(clientAddr);
    SOCKET clientSocket = ::accept(sock_, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

    if (clientSocket == INVALID_SOCKET) {
        return std::unexpected(getLastError(SocketError::Type::Connection));
    }

    auto clientWinsockSocket = std::make_shared<WinsockSocket>();
    clientWinsockSocket->sock_ = clientSocket;
    clientWinsockSocket->initialized_ = true;

    return clientWinsockSocket;
}

SocketError WinsockSocket::connect(const std::string& address, uint16_t port) {
    if (!initialized_) {
        return { SocketError::Type::Initialization, 0 };
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        return getLastError(SocketError::Type::Connection);
    }

    if (::connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        return getLastError(SocketError::Type::Connection);
    }

    return SocketError::success();
}

SocketError WinsockSocket::send(const std::vector<uint8_t>& data) {
    if (!initialized_) {
        return { SocketError::Type::Initialization, 0 };
    }

    int result = ::send(sock_, reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()), 0);

    if (result == SOCKET_ERROR) {
        return getLastError(SocketError::Type::Send);
    }

    return SocketError::success();
}

std::expected<std::vector<uint8_t>, SocketError> WinsockSocket::receive(size_t maxSize) {
    if (!initialized_) {
        return std::unexpected(SocketError{ SocketError::Type::Initialization, 0 });
    }

    std::vector<uint8_t> buffer(maxSize);
    int bytesReceived = recv(sock_, reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(maxSize), 0);

    if (bytesReceived == SOCKET_ERROR) {
        return std::unexpected(getLastError(SocketError::Type::Receive));
    }

    buffer.resize(bytesReceived);
    return buffer;
}

void WinsockSocket::close() {
    if (initialized_ && sock_ != INVALID_SOCKET) {
        shutdown(sock_, SD_BOTH);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        initialized_ = false;
    }
}

int WinsockSocket::setTimeout() {
    // Set Socket Options
    struct timeval timeout;
    timeout.tv_sec = 60;  // 60 Second
    timeout.tv_usec = 0;

    if (setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout), sizeof(timeout)) < 0) {
        return 1;
    }

    if (setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO,
        reinterpret_cast<const char*>(&timeout), sizeof(timeout)) < 0) {
        return 1;
    }
    return 0;
}

bool WinsockSocket::isSameSocket(const std::shared_ptr<Socket>& other) const {
    if (auto* otherWinsock = dynamic_cast<const WinsockSocket*>(other.get())) {
        return sock_ == otherWinsock->sock_;
    }
    return false;
}

// Initialize Static Member
bool WinsockSocket::winsockInitialized_ = false;