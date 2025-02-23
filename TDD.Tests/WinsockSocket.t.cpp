#include "pch.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "WinsockSocket.h"
#include "Socket.h"
#include <winsock2.h>

namespace WinSockImplementation {
    class WinsockSocketTest : public ::testing::Test {
    protected:
        std::unique_ptr<WinsockSocket> socket;

        void SetUp() override {
            socket = std::make_unique<WinsockSocket>();
            socket->init();
        }

        void TearDown() override {
            socket->close();
        }
    };

    TEST_F(WinsockSocketTest, BindToValidPort) {
        auto result = socket->bind("127.0.0.1", 8080);
        EXPECT_EQ(result.type, SocketError::Type::None);
    }

    TEST_F(WinsockSocketTest, BindToInvalidAddress) {
        auto result = socket->bind("invalid_address", 8080);
        EXPECT_EQ(result.type, SocketError::Type::Bind);
    }
}