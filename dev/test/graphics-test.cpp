#include "../3rd-party/catch2/catch2.hpp"
#include "test-instance.h"
#include "shader/clear-screen.vert.spv.h"
#include "shader/clear-screen.frag.spv.h"
#include "rdc.h"

TEST_CASE("clear-screen") {
    using namespace rapid_vulkan;
    auto device = TestVulkanInstance::device.get();
    auto gi     = device->gi();
    auto w      = uint32_t(128);
    auto h      = uint32_t(72);
    auto color  = Image(Image::ConstructParameters {{"color"}, gi}.set2D(w, h).renderTarget());
    auto pass   = RenderPass(RenderPass::ConstructParameters {{"clear-screen"}, gi}.simple({color.desc().format}));
    auto fb     = Framebuffer(Framebuffer::ConstructParameters {{"clear-screen"}, gi, pass}.addImage(color));
    auto vs     = Shader(Shader::ConstructParameters {{"clear-screen-vs"}}.setGi(gi).setSpirv(clear_screen_vert));
    auto fs     = Shader(Shader::ConstructParameters {{"clear-screen-fs"}, gi}.setSpirv(clear_screen_frag));
    auto q      = CommandQueue({{"main"}, gi, device->graphics()->family(), device->graphics()->index()});

    // create the graphics pipeline
    auto gcp = GraphicsPipeline::ConstructParameters {{"clear-screen"}}.setRenderPass(pass).setVS(&vs).setFS(&fs);
    gcp.viewports.push_back(vk::Viewport(0.f, 0.f, (float) w, (float) h, 0.f, 1.f));
    gcp.scissors.push_back(vk::Rect2D({0, 0}, {w, h}));
    auto p = GraphicsPipeline(gcp);

    auto render = [&](std::array<float, 4> clearColor, bool drawTriangle) {
        auto c = q.begin("clear-screen");
        // clear the screen to green
        Barrier()
            .i(color, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageLayout::eUndefined,
               vk::ImageLayout::eColorAttachmentOptimal, vk::ImageAspectFlagBits::eColor)
            .cmdWrite(c);
        std::array cv = {vk::ClearValue().setColor(vk::ClearColorValue(clearColor)), vk::ClearValue().setDepthStencil({1, 0})};
        pass.cmdBegin(c, vk::RenderPassBeginInfo {pass, fb, vk::Rect2D({0, 0}, {w, h})}.setClearValues(cv));
        if (drawTriangle) p.cmdDraw(c, GraphicsPipeline::DrawParameters {}.setNonIndexed(3)); // draw a full screen blue triangle.
        pass.cmdEnd(c);
        q.submit({c});
        q.wait();
    };

    // case 1. clear only.
    SECTION("clear-only") {
        render(std::array<float, 4> {0.f, 1.f, 0.f, 1.f}, false); // clear to green
        auto pixels = color.readContent({});
        REQUIRE(pixels.storage.size() >= 4);
        CHECK(0xFF00FF00 == *(const uint32_t *) pixels.storage.data()); // verify that the screen is green.
    }

    // case 2: draw full screen triangle
    SECTION("full-screen-triangle") {
        RenderDocCapture rdc;
        rdc.begin("clear-screen-section-2");
        render(std::array<float, 4> {1.f, 0.f, 0.f, 1.f}, true); // clear to red, then draw a full screen blue triangle.
        auto pixels = color.readContent({});
        REQUIRE(pixels.storage.size() >= 4);
        CHECK(0xFFFF0000 == *(const uint32_t *) pixels.storage.data());
        rdc.end();
    }
}
