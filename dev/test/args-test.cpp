#include "test-instance.h"
#include "3rd-party/catch2/catch2.hpp"
#include "shader/argument-test.comp.spv.h"
#include "rdc.h"

TEST_CASE("cs-buffer-args") {
    using namespace rapid_vulkan;
    auto dev  = TestVulkanInstance::device.get();
    auto gi   = dev->gi();
    auto noop = Shader({{"cs-buffer-args"}, gi, {sizeof(argument_test_comp) / sizeof(uint32_t), (const uint32_t *) argument_test_comp}});
    auto p    = ComputePipeline({{"cs-buffer-args"}, noop});
    auto rdc  = RenderDocCapture();

    if (rdc) rdc.begin("cs-buffer-args");

    // create buffers and argument pack
    auto b1 = Buffer({{"buf1"}, TestVulkanInstance::device->gi(), 4, vk::BufferUsageFlagBits::eStorageBuffer});
    b1.setContent(Buffer::SetContentParameters {}.setData(vk::ArrayProxy<const uint32_t> {0xabcd1234}));
    auto b2 = Buffer({{"buf2"}, TestVulkanInstance::device->gi(), 4, vk::BufferUsageFlagBits::eStorageBuffer});
    b2.setContent(Buffer::SetContentParameters {}.setData(vk::ArrayProxy<const uint32_t> {0xbadbeef}));
    auto ap = ArgumentPack({"cs-buffer-args"});
    ap.b("InputBuffer", {{b1}});
    ap.b("OutputBuffer", {{b2}});

    // run the compute shader to copy data from b1 to b2
    auto q = CommandQueue({{"cs-buffer-args"}, gi, dev->graphics()->family(), dev->graphics()->index()});
    if (auto c = q.begin("cs-buffer-args")) {
        p.cmdBind(c, ap);
        p.cmdDispatch(c, {1, 1, 1});
        q.submit(c);
    }
    q.wait();

    if (rdc) rdc.end();

    // read contents of b2
    auto c2 = b2.readContent({});
    REQUIRE(c2.size() == 4);
    REQUIRE(*(const uint32_t *) c2.data() == 0xabcd1234);
}