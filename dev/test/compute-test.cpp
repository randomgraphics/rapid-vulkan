#include "test-instance.h"
#include "../3rd-party/catch2/catch.hpp"
#include "shader/argument-test.comp.spv.h"
#include "shader/noop.comp.spv.h"
#include "rdc.h"

TEST_CASE("noop-compute") {
    using namespace rapid_vulkan;
    auto device = TestVulkanInstance::device.get();
    auto gi     = device->gi();
    auto noop   = new Shader(Shader::ConstructParameters {{"noop"}, gi}.setSpirv(noop_comp));
    auto p      = ComputePipeline({{"noop"}, noop});

    // This is to verify that pipeline object is not keeping reference to the shader object after the construction.
    delete noop;

    auto q = CommandQueue({{"main"}, gi, device->graphics()->family(), device->graphics()->index()});
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
    auto p    = Ref(new ComputePipeline({{"cs-buffer-args"}, &noop}));

    auto rdc = RenderDocCapture();
    if (rdc) rdc.begin("cs-buffer-args");

    // store the number of buffer objects currently alive.
    auto numBuffers = Buffer::instanceCount();

    // create buffers and drawable
    auto b1 = Ref(new Buffer({{"buf1"}, TestVulkanInstance::device->gi(), 4, vk::BufferUsageFlagBits::eStorageBuffer}));
    b1->setContent(Buffer::SetContentParameters {}.setData(vk::ArrayProxy<const float> {1.0f}));
    auto b2 = Ref(new Buffer({{"buf2"}, TestVulkanInstance::device->gi(), 4, vk::BufferUsageFlagBits::eStorageBuffer}));
    b2->setContent(Buffer::SetContentParameters {}.setData(vk::ArrayProxy<const uint32_t> {0xbadbeef}));
    auto ap = Ref(new Drawable({{"cs-buffer-args"}, p}));
    ap->b({0, 0}, {{b1}});
    ap->b({0, 1}, {{b2}});
    ap->c(0, vk::ArrayProxy<const float> {1.0f});
    ap->dispatch(ComputePipeline::DispatchParameters {1, 1, 1});

    // This is to verify that the drawable object is keeping buffer objects alive.
    b1 = nullptr;
    REQUIRE(Buffer::instanceCount() == numBuffers + 2);

    // run the compute shader to copy data from b1 to b2
    auto q = dev->graphics();
    REQUIRE(q);
    if (auto c = q->begin("cs-buffer-args")) {
        auto pack = ap->compile();
        ap        = nullptr; // release drawable object to verify that all resource are kept alive by the draw pack object.
        c.render(pack);
        pack = nullptr; // release draw pack object to verify that all resource are kept alive by the command buffer.
        q->submit({c});
    }

    REQUIRE(Buffer::instanceCount() == numBuffers + 2);

    // Wait for the queue to finish. Once it returns, all resources used by it should be released.
    q->waitIdle();

    // b1 should be released after the command queue is idle.
    REQUIRE(Buffer::instanceCount() == numBuffers + 1);

    // read contents of b2
    auto c2 = b2->readContent({});
    if (rdc) rdc.end();
    REQUIRE(c2.size() == 4);
    REQUIRE(*(const float *) c2.data() == 2.0f);
}