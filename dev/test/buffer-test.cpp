#include "test-instance.h"
#include "3rd-party/catch2.hpp"

TEST_CASE("buffer-dtor") {
    using namespace rapid_vulkan;
    auto buffer = Buffer({"dtor-test", TestVulkanInstance::device->gi(), 8});
}

TEST_CASE("buffer-read-write") {
    using namespace rapid_vulkan;

    auto buffer = Buffer({"buf1", TestVulkanInstance::device->gi(), 8});
    REQUIRE(buffer.handle());
    REQUIRE(buffer.desc().memory);

    auto content = Buffer::SetContentParameters();
    content.setQueue(*TestVulkanInstance::device->graphics());

    // set the first 4 bytes to 0xbadbabe
    uint32_t tag1 = 0xbadbabe;
    content.setData(&tag1, 4);
    content.setOffset(0);
    buffer.setContent(content);

    // set the last 4 bytes to 0xdeadbeef
    uint32_t tag2 = 0xdeadbeef;
    content.setData(&tag2, 4);
    content.setOffset(4);
    buffer.setContent(content);

    // ready the whole buffer back.
    auto readback = buffer.readContent(Buffer::ReadParameters{}.setQueue(*TestVulkanInstance::device->graphics()));
    REQUIRE(readback.size() == 8);
    auto p32 = (const uint32_t *)readback.data();
    CHECK(0xbadbabe == p32[0]);
    CHECK(0xdeadbeef == p32[1]);
}

TEST_CASE("buffer-write-overflow") {
    using namespace rapid_vulkan;

    auto buffer = Buffer({{}, TestVulkanInstance::device->gi(), 4});

    auto content = Buffer::SetContentParameters();
    content.setQueue(*TestVulkanInstance::device->graphics());

    // make content longer than the buffer.
    std::vector<uint32_t> v (5, 0xbadbabe);
    content.setData(v.data(), v.size());

    // set content should not throw any error.
    buffer.setContent(content);
}
