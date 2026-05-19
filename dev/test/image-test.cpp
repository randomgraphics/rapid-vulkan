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
    image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(pixels));

    // readContent() reads the internal state for current layout (auto-updated by setContent).
    auto read = image.readContent(Image::ReadContentParameters {}.setQueue(*dev->graphics()));
    REQUIRE(read.format == cp.info.format);
    REQUIRE(read.storage.size() == 4 * cp.info.extent.width * cp.info.extent.height);
    auto p = (const uint32_t *) read.storage.data();
    CHECK(p[0] == pixels[0]);
    CHECK(p[1] == pixels[1]);
    CHECK(p[2] == pixels[2]);
    CHECK(p[3] == pixels[3]);
}

TEST_CASE("image-state-initial") {
    using namespace rapid_vulkan;
    auto cp    = Image::ConstructParameters {{"st"}, TestVulkanInstance::device->gi()}.set2D(4, 4);
    auto image = Image(cp);

    // Freshly constructed image starts in UNDEFINED layout on every plane.
    const auto & st = image.getState();
    CHECK(st.validAspects == vk::ImageAspectFlagBits::eColor);
    const auto * plane = st.get(0, 0, vk::ImageAspectFlagBits::eColor);
    REQUIRE(plane);
    CHECK(plane->layout == vk::ImageLayout::eUndefined);
    // Out-of-range queries return nullptr.
    CHECK(!st.get(99, 0, vk::ImageAspectFlagBits::eColor));
    CHECK(!st.get(0, 0, vk::ImageAspectFlagBits::eDepth)); // color image has no depth plane
}

TEST_CASE("image-state-after-setContent") {
    using namespace rapid_vulkan;
    auto dev   = TestVulkanInstance::device.get();
    auto cp    = Image::ConstructParameters {{"sc"}, dev->gi()}.set2D(2, 2);
    auto image = Image(cp);

    const uint32_t pixels[] = {1, 2, 3, 4};
    image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(pixels));

    // setContent() transitions mip 0, layer 0 to TRANSFER_DST_OPTIMAL.
    const auto * plane = image.getState().get(0, 0, vk::ImageAspectFlagBits::eColor);
    REQUIRE(plane);
    CHECK(plane->layout == vk::ImageLayout::eTransferDstOptimal);
}

TEST_CASE("image-state-after-readContent") {
    using namespace rapid_vulkan;
    auto dev   = TestVulkanInstance::device.get();
    auto cp    = Image::ConstructParameters {{"rc"}, dev->gi()}.set2D(2, 2);
    auto image = Image(cp);

    const uint32_t pixels[] = {1, 2, 3, 4};
    image.setContent(Image::SetContentParameters {}.setQueue(*dev->graphics()).setPixels(pixels));
    image.readContent(Image::ReadContentParameters {}.setQueue(*dev->graphics()));

    // readContent() transitions entire image to TRANSFER_SRC_OPTIMAL.
    const auto * plane = image.getState().get(0, 0, vk::ImageAspectFlagBits::eColor);
    REQUIRE(plane);
    CHECK(plane->layout == vk::ImageLayout::eTransferSrcOptimal);
}

TEST_CASE("image-setState") {
    using namespace rapid_vulkan;
    auto cp    = Image::ConstructParameters {{"ss"}, TestVulkanInstance::device->gi()}.set2D(4, 4).setLevels(2);
    auto image = Image(cp);

    // Set mip 1 to TRANSFER_DST.
    image.setState(Image::State::PlaneState::TRANSFER_DST(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 1, 1, 0, 1));

    // Mip 0 is still UNDEFINED.
    const auto * mip0 = image.getState().get(0, 0, vk::ImageAspectFlagBits::eColor);
    REQUIRE(mip0);
    CHECK(mip0->layout == vk::ImageLayout::eUndefined);

    // Mip 1 is now TRANSFER_DST_OPTIMAL.
    const auto * mip1 = image.getState().get(1, 0, vk::ImageAspectFlagBits::eColor);
    REQUIRE(mip1);
    CHECK(mip1->layout == vk::ImageLayout::eTransferDstOptimal);
}
