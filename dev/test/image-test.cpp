#include "test-instance.h"
#include "../3rd-party/catch2/catch.hpp"

TEST_CASE("image-smoke") {
    using namespace rapid_vulkan;
    // default parameter should generate an valid image.
    auto cp    = Image::ConstructParameters {{}, TestVulkanInstance::device->gi()};
    auto image = Image(cp);
}
TEST_CASE("image-view") {
    using namespace rapid_vulkan;
    auto cp    = Image::ConstructParameters {{}, TestVulkanInstance::device->gi()};
    auto image = Image(cp);
    auto view  = image.getView({});
    CHECK(view);
}

TEST_CASE("image-read-write") {
    using namespace rapid_vulkan;
    auto dev = TestVulkanInstance::device.get();

    // create a 2x2 image
    auto cp    = Image::ConstructParameters {{"m1"}, dev->gi()}.set2D(2, 2);
    auto image = Image(cp);

    // set image content
    const uint32_t pixels[] = {0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff};
    REQUIRE(image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(sizeof(pixels), pixels)));

    // readContent() reads the image into a staging buffer and restores it to SHADER_READ_ONLY_OPTIMAL.
    auto read = image.readContent(Image::ReadContentParameters {}.setQueue(*dev->graphics()));
    REQUIRE(read.format == cp.info.format);
    REQUIRE(read.storage.size() == 4 * cp.info.extent.width * cp.info.extent.height);
    auto p = (const uint32_t *) read.storage.data();
    CHECK(p[0] == pixels[0]);
    CHECK(p[1] == pixels[1]);
    CHECK(p[2] == pixels[2]);
    CHECK(p[3] == pixels[3]);
}

TEST_CASE("image-valid-aspects") {
    using namespace rapid_vulkan;
    auto cp    = Image::ConstructParameters {{"st"}, TestVulkanInstance::device->gi()}.set2D(4, 4);
    auto image = Image(cp);

    CHECK(image.validAspects() == vk::ImageAspectFlagBits::eColor);
}

TEST_CASE("image-setContent-size-validation") {
    using namespace rapid_vulkan;
    auto dev = TestVulkanInstance::device.get();

    auto cp    = Image::ConstructParameters {{"sc-val"}, dev->gi()}.set2D(4, 4);
    auto image = Image(cp);

    // 4x4 RGBA8 requires 4 * 4 * 4 = 64 bytes.
    std::vector<uint32_t> pixels(16, 0xffffffff);

    // Size smaller than required must fail.
    CHECK(!image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(63, pixels.data())));

    // Exact size must succeed.
    CHECK(image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(64, pixels.data())));

    // Larger size must succeed.
    CHECK(image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(128, pixels.data())));
}

TEST_CASE("image-setContent-mip-layers") {
    using namespace rapid_vulkan;
    auto dev = TestVulkanInstance::device.get();

    auto cp    = Image::ConstructParameters {{"sc-mips"}, dev->gi()}.set2D(4, 4).setLevels(2).setLayers(2);
    auto image = Image(cp);

    std::vector<uint32_t> mip0(16, 0x11111111);
    std::vector<uint32_t> mip1(4, 0x22222222);

    // Upload layer 0 mip 0 and mip 1
    CHECK(image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(64, mip0.data()).setMipLevel(0).setArrayLayer(0)));
    CHECK(image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(16, mip1.data()).setMipLevel(1).setArrayLayer(0)));

    // Upload layer 1 mip 0 and mip 1
    CHECK(image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(64, mip0.data()).setMipLevel(0).setArrayLayer(1)));
    CHECK(image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(16, mip1.data()).setMipLevel(1).setArrayLayer(1)));

    // Invalid layer or mip must fail
    CHECK(!image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(64, mip0.data()).setMipLevel(2)));
    CHECK(!image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(64, mip0.data()).setArrayLayer(2)));
}
