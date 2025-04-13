#include "pch.h"
//#include <gtest/gtest.h>
//#include <gmock/gmock.h>
//#include "gmock/gmock-spec-builders.h"
//#include "gmock/internal/gmock-internal-utils.h"
//#include "gmock/internal/gmock-pp.h"
//#include "Socket.h"
//#include <winsock2.h>
//
//namespace SocketTests {
//    class MockSocket : public Socket {
//    public:
//        MOCK_METHOD(SocketError, init, (), (override));
//        MOCK_METHOD(SocketError, bind, (const std::pmr::string&, uint16_t), (override));
//        MOCK_METHOD(SocketError, listen, (int), (override));
//        MOCK_METHOD((std::expected<std::shared_ptr<Socket>, SocketError>), accept, (), (override));
//        MOCK_METHOD(SocketError, connect, (const std::string&, uint16_t), (override));
//        MOCK_METHOD(SocketError, send, (const std::vector<uint8_t>&), (override));
//        MOCK_METHOD((std::expected<std::vector<uint8_t>, SocketError>), receive, (size_t), (override));
//        MOCK_METHOD(void, close, (), (override));
//        MOCK_METHOD(void, cleanup, (), (override));
//        MOCK_METHOD(int, setTimeout, (), (override));
//        MOCK_METHOD(bool, isSameSocket, (const std::shared_ptr<Socket>&), (const, override));
//    };
//
//    TEST(SocketTests, InitializationSuccess) {
//        MockSocket socket;
//        EXPECT_CALL(socket, init())
//            .WillOnce(testing::Return(SocketError{ SocketError::Type::None, 0 }));
//
//        auto result = socket.init();
//        EXPECT_EQ(result.type, SocketError::Type::None);
//        EXPECT_EQ(result.internalCode, 0);
//    }
//
//    TEST(SocketTests, InitializationFailure) {
//        MockSocket socket;
//        EXPECT_CALL(socket, init())
//            .WillOnce(testing::Return(SocketError{ SocketError::Type::Initialization, WSAEINVAL }));
//
//        auto result = socket.init();
//        EXPECT_EQ(result.type, SocketError::Type::Initialization);
//        EXPECT_EQ(result.internalCode, WSAEINVAL);
//    }
//
//    TEST(SocketTests, ReceiveSuccess) {
//        MockSocket socket;
//        std::vector<uint8_t> expectedData = { 1, 2, 3, 4 };
//
//        EXPECT_CALL(socket, receive(1024))
//            .WillOnce(testing::Return(std::expected<std::vector<uint8_t>, SocketError>(expectedData)));
//
//        auto result = socket.receive(1024);
//        ASSERT_TRUE(result.has_value());
//        EXPECT_EQ(result.value(), expectedData);
//    }
//
//    TEST(SocketTests, ReceiveFailure) {
//        MockSocket socket;
//
//        EXPECT_CALL(socket, receive(1024))
//            .WillOnce(testing::Return(std::unexpected(SocketError{ SocketError::Type::Receive, WSAECONNRESET })));
//
//        auto result = socket.receive(1024);
//        ASSERT_FALSE(result.has_value());
//        EXPECT_EQ(result.error().type, SocketError::Type::Receive);
//        EXPECT_EQ(result.error().internalCode, WSAECONNRESET);
//    }
//}