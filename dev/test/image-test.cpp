#include "test-instance.h"
#include "3rd-party/catch2/catch2.hpp"

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
    image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(pixels));

    // then read it back
    auto read = image.readContent(Image::ReadContentParameters {}.setQueue(*dev->graphics()));
    REQUIRE(read.format == cp.info.format);
    REQUIRE(read.storage.size() == 4 * cp.info.extent.width * cp.info.extent.height);
    auto p = (const uint32_t *) read.storage.data();
    CHECK(p[0] == pixels[0]);
    CHECK(p[1] == pixels[1]);
    CHECK(p[2] == pixels[2]);
    CHECK(p[3] == pixels[3]);
}
