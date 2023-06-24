#include "../3rd-party/catch2/catch.hpp"
#include "test-instance.h"
#include "shader/full-screen.vert.spv.h"
#include "shader/passthrough-2d.vert.spv.h"
#include "shader/blue-color.frag.spv.h"
#include "rdc.h"

TEST_CASE("clear-screen") {
    using namespace rapid_vulkan;
    auto device = TestVulkanInstance::device.get();
    auto gi     = device->gi();
    auto w      = uint32_t(128);
    auto h      = uint32_t(72);
    auto sw     = Swapchain(Swapchain::ConstructParameters {{"vertex-buffer-test"}}.setDevice(*device).setDimensions(w, h));
    auto vs     = Shader(Shader::ConstructParameters {{"clear-screen-vs"}}.setGi(gi).setSpirv(full_screen_vert));
    auto fs     = Shader(Shader::ConstructParameters {{"clear-screen-fs"}, gi}.setSpirv(blue_color_frag));
    auto q      = CommandQueue({{"main"}, gi, device->graphics()->family(), device->graphics()->index()});

    // create the graphics pipeline
    auto gcp = GraphicsPipeline::ConstructParameters {{"clear-screen"}}.setRenderPass(sw.renderPass()).setVS(&vs).setFS(&fs);
    gcp.viewports.push_back(vk::Viewport(0.f, 0.f, (float) w, (float) h, 0.f, 1.f));
    gcp.scissors.push_back(vk::Rect2D({0, 0}, {w, h}));
    auto p = GraphicsPipeline(gcp);

    auto render = [&](std::array<float, 4> clearColor, bool drawTriangle) {
        auto c = q.begin("clear-screen");
        // clear the screen to green
        // Barrier()
        //     .i(color, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined,
        //        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageAspectFlagBits::eColor)
        //     .cmdWrite(c);
        auto bp = Swapchain::BeginRenderPassParameters {}.setClearColorF(clearColor).setClearDepth(1.f, 0);
        sw.cmdBeginBuiltInRenderPass(c, bp);
        if (drawTriangle) p.cmdDraw(c, GraphicsPipeline::DrawParameters {}.setNonIndexed(3)); // draw a full screen blue triangle.
        sw.cmdEndBuiltInRenderPass(c);
        q.submit({c}).wait();
    };

    // case 1. clear only.
    SECTION("clear-only") {
        render(std::array<float, 4> {0.f, 1.f, 0.f, 1.f}, false); // clear to green
        auto pixels = sw.currentFrame().backbuffer->image->readContent({});
        REQUIRE(pixels.storage.size() >= 4);
        CHECK(0xFF00FF00 == *(const uint32_t *) pixels.storage.data()); // verify that the screen is green.
    }

    // case 2: draw full screen triangle
    SECTION("full-screen-triangle") {
        RenderDocCapture rdc;
        rdc.begin("clear-screen-section-2");
        render(std::array<float, 4> {1.f, 0.f, 0.f, 1.f}, true); // clear to red, then draw a full screen blue triangle.
        auto pixels = sw.currentFrame().backbuffer->image->readContent({});
        REQUIRE(pixels.storage.size() >= 4);
        CHECK(0xFFFF0000 == *(const uint32_t *) pixels.storage.data());
        rdc.end();
    }
}

TEST_CASE("vertex-buffer") {
    using namespace rapid_vulkan;
    auto   device = TestVulkanInstance::device.get();
    auto   gi     = device->gi();
    auto   w      = uint32_t(128);
    auto   h      = uint32_t(72);
    auto   sw     = Swapchain(Swapchain::ConstructParameters {{"vertex-buffer-test"}}.setDevice(*device).setDimensions(w, h));
    auto & q      = sw.graphics();
    auto   vs     = Shader(Shader::ConstructParameters {{"vertex-buffer-test"}}.setGi(gi).setSpirv(passthrough_2d_vert));
    auto   fs     = Shader(Shader::ConstructParameters {{"vertex-buffer-test"}, gi}.setSpirv(blue_color_frag));
    auto   p      = GraphicsPipeline(GraphicsPipeline::ConstructParameters {{"vertex-buffer-test"}}
                                         .setRenderPass(sw.renderPass())
                                         .setVS(&vs)
                                         .setFS(&fs)
                                         .addStaticViewportAndScissor(0, 0, w, h)
                                         .addVertexAttribute(0, 0, vk::Format::eR32G32Sfloat)
                                         .addVertexBuffer(2 * sizeof(float)));
    auto   vb     = Buffer(Buffer::ConstructParameters {{"vertex-buffer-test"}, gi}.setSize(6 * sizeof(float)).setVertex());

    // set content of vertex buffer to a triangle that covers the lower left half of the screen.
    vb.setContent(Buffer::SetContentParameters {}.setQueue(q).setData<float>({-1.f, 1.f, 1.f, 1.f, -1.f, -1.f}));

    // render the triangle
    RenderDocCapture rdc;
    rdc.begin("vertex-buffer-test");
    auto f = sw.currentFrame();
    auto c = q.begin("vertex-buffer-test");
    sw.cmdBeginBuiltInRenderPass(c, Swapchain::BeginRenderPassParameters {}.setClearColorF({0.0f, 1.0f, 0.0f, 1.0f})); // clear to green
    c.handle().bindVertexBuffers(0, {vb.handle()}, {0});                                                               // bind the vertex buffer
    p.cmdDraw(c, GraphicsPipeline::DrawParameters {}.setNonIndexed(3));                                                // then draw a blue triangle.
    sw.cmdEndBuiltInRenderPass(c);
    q.submit({c, {}, {f.imageAvailable}, {f.renderFinished}}).wait();
    rdc.end();

    // read content of back buffer.
    auto pixels = f.backbuffer->image->readContent({});

    // Since we clear the whole screen to green, then draw a blue triangle to cover the lower left half of the screen,
    // then pixel (1, 0) should be green, and pixel (0, 1) should be blue.
    auto ptr = (const uint32_t *) pixels.storage.data();
    CHECK(0xFF00FF00 == ptr[1]);
    CHECK(0xFFFF0000 == ptr[w]);
}