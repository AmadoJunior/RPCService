#include "pch.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <unordered_map>
#include <map>
#include <vector>
#include "msgpack/msgpack.hpp"

struct Example {
    std::map<std::string, bool> map;

    template<class T>
    void pack(T& pack) {
        pack(map);
    }
};

TEST(WEBSITE, EXAMPLE) {
    Example example{ {{"compact", true}, {"schema", false}} };
    auto data = msgpack::pack(example);
    std::vector<uint8_t> mock = { 0x82, 0xa7, 0x63, 0x6f, 0x6d, 0x70, 0x61, 0x63, 0x74, 0xc3, 0xa6, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0xc2 };
    ASSERT_EQ(data.size(), 18);
    ASSERT_EQ(data, mock);

    ASSERT_EQ(example.map, msgpack::unpack<Example>(data).map);
}

TEST(UNPACK, WITH_ERROR_HANDLING) {
    Example example{ {{"compact", true}, {"schema", false}} };
    auto data = std::vector<uint8_t>{ 0x82, 0xa7, 0x63, 0x6f, 0x6d, 0x70, 0x61, 0x63, 0x74, 0xc3, 0xa6, 0x73, 0x63 };
    std::error_code ec{};
    auto unpacked_object = msgpack::unpack<Example>(data, ec);
    if (ec && ec == msgpack::UnpackerError::OutOfRange)
        SUCCEED();
    else
        FAIL();
}