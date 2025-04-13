#include "WinsockSocket.h"
#include "Socket.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <expected>
#include <memory_resource> // Add PMR header

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

WinsockSocket::WinsockSocket(std::pmr::memory_resource* resource)
    : sock_(INVALID_SOCKET),
    initialized_(false),
    resource_(resource) {
}

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

    // Allow Socket Reuse
    BOOL opt = TRUE;
    if (setsockopt(sock_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
        reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
        return getLastError(SocketError::Type::Initialization);
    }
    if (setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY,
        reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
        return getLastError(SocketError::Type::Initialization);
    }

    initialized_ = true;
    return SocketError::success();
}

std::pmr::memory_resource* WinsockSocket::getMemoryResource() const {
    return resource_;
}

SocketError WinsockSocket::bind(const std::pmr::string& address, uint16_t port) {
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

    auto clientWinsockSocket = std::make_shared<WinsockSocket>(resource_);
    clientWinsockSocket->sock_ = clientSocket;
    clientWinsockSocket->initialized_ = true;

    return clientWinsockSocket;
}

SocketError WinsockSocket::connect(const std::pmr::string& address, uint16_t port) {
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

SocketError WinsockSocket::send(const std::pmr::vector<uint8_t>& data) {
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

SocketError WinsockSocket::setNonBlocking() {
    if (!initialized_) return{ SocketError::Type::Initialization, 0 };;

    // Set the socket to non-blocking mode
    u_long mode = 1;  // 1 = non-blocking, 0 = blocking
    int result = ioctlsocket(sock_, FIONBIO, &mode);

    if (result == SOCKET_ERROR) {
        // Handle error - in practice you might want to log this
        int error = WSAGetLastError();
        return { SocketError::Type::Initialization, error };
    }
    return SocketError::success();
}

std::expected<std::pmr::vector<uint8_t>, SocketError> WinsockSocket::receive(size_t maxSize) {
    if (!initialized_) {
        return std::unexpected(SocketError{ SocketError::Type::Initialization, 0 });
    }

    std::pmr::vector<uint8_t> buffer(maxSize, resource_);
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
        struct linger lin;
        lin.l_onoff = 1;    // Enable linger
        lin.l_linger = 0;   // Zero timeout - force abort of connection
        setsockopt(sock_, SOL_SOCKET, SO_LINGER, (const char*)&lin, sizeof(lin));

        shutdown(sock_, SD_BOTH);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        initialized_ = false;
    }
}

int WinsockSocket::setTimeout() {
    // Set Socket Options
    struct timeval timeout;
    timeout.tv_sec = 5;  // 5 Second
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