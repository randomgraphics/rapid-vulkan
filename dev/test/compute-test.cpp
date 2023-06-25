#include "test-instance.h"
#include "../3rd-party/catch2/catch.hpp"
#include "shader/argument-test.comp.spv.h"
#include "shader/noop.comp.spv.h"
#include "rdc.h"

TEST_CASE("noop-compute") {
    using namespace rapid_vulkan;
    auto device = TestVulkanInstance::device.get();
    auto gi     = device->gi();
    auto noop   = Shader(Shader::ConstructParameters {{"noop"}, gi}.setSpirv(noop_comp));
    auto p      = ComputePipeline({{"noop"}, &noop});
    auto q      = CommandQueue({{"main"}, gi, device->graphics()->family(), device->graphics()->index()});
    if (auto c = q.begin("main")) {
        p.cmdDispatch(c, {1, 1, 1});
        q.submit({c});
    }
    q.waitIdle();
}

TEST_CASE("cs-buffer-args") {
    using namespace rapid_vulkan;
    auto dev  = TestVulkanInstance::device.get();
    auto gi   = dev->gi();
    auto noop = Shader(Shader::ConstructParameters {{"cs-buffer-args"}, gi}.setSpirv(argument_test_comp));
    auto p    = ComputePipeline({{"cs-buffer-args"}, &noop});
    p.doNotDeleteOnZeroRef();

    auto rdc = RenderDocCapture();
    if (rdc) rdc.begin("cs-buffer-args");

    // create buffers and argument pack
    auto b1 = Buffer({{"buf1"}, TestVulkanInstance::device->gi(), 4, vk::BufferUsageFlagBits::eStorageBuffer});
    b1.setContent(Buffer::SetContentParameters {}.setData(vk::ArrayProxy<const float> {1.0f}));
    auto b2 = Buffer({{"buf2"}, TestVulkanInstance::device->gi(), 4, vk::BufferUsageFlagBits::eStorageBuffer});
    b2.setContent(Buffer::SetContentParameters {}.setData(vk::ArrayProxy<const uint32_t> {0xbadbeef}));
    auto ap = Drawable({{"cs-buffer-args"}, &p});
    ap.b({0, 0}, {{b1}});
    ap.b({0, 1}, {{b2}});
    ap.c(0, vk::ArrayProxy<const float> {1.0f});
    ap.dispatch(ComputePipeline::DispatchParameters {1, 1, 1});

    // run the compute shader to copy data from b1 to b2
    auto q = dev->graphics();
    REQUIRE(q);
    if (auto c = q->begin("cs-buffer-args")) {
        c.render(*ap.compile());
        q->submit({c});
    }
    q->waitIdle();

    // read contents of b2
    auto c2 = b2.readContent({});
    if (rdc) rdc.end();
    REQUIRE(c2.size() == 4);
    REQUIRE(*(const float *) c2.data() == 2.0f);
}