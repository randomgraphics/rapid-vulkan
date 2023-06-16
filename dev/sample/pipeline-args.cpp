/*
    This file demonstrates the basic usage of feeding data to graphics pipeline via ArgumentPack class.
*/
#include "../rv.h"
#include <iostream>
#include <cmath>

namespace pipeline {

#include "shader/pipeline.vert.spv.h"
#include "shader/pipeline.frag.spv.h"

struct GLFWInit {
    vk::Instance inst;
    GLFWwindow * window  = nullptr;
    VkSurfaceKHR surface = nullptr;
    GLFWInit(bool headless, vk::Instance instance, uint32_t w, uint32_t h, const char * title): inst(instance) {
        if (headless) return; // do nothing in headless mode.
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow((int) w, (int) h, title, nullptr, nullptr);
        RVI_REQUIRE(window, "Failed to create GLFW window.");
        glfwCreateWindowSurface(instance, window, nullptr, &surface);
        RVI_REQUIRE(surface, "Failed to create surface.");
    }
    ~GLFWInit() {
        if (surface) inst.destroySurfaceKHR(surface), surface = VK_NULL_HANDLE;
        if (window) glfwDestroyWindow(window), window = nullptr;
        glfwTerminate();
    }
    void show() {
        if (window) glfwShowWindow(window);
    }
};

struct Options {
    bool headless = false;
};

void entry(const Options & options) {
    // Standard boilerplate of creating instance, device, swapchain, etc. It is basicaly the same as simple-triangle.cpp.
    using namespace rapid_vulkan;
    auto w        = uint32_t(1280);
    auto h        = uint32_t(720);
    auto instance = Instance(Instance::ConstructParameters {}.setValidation(Instance::BREAK_ON_VK_ERROR));
    auto glfw     = GLFWInit(options.headless, instance, w, h, "pipeline-args");
    auto device   = Device(instance.dcp().setSurface(glfw.surface));
    auto gi       = device.gi();
    auto q        = CommandQueue({{"main"}, gi, device.graphics()->family(), device.graphics()->index()});
    auto sw       = Swapchain(Swapchain::ConstructParameters {{"swapchain"}}.setDevice(device).setDimensions(w, h));
    auto vs       = Shader(Shader::ConstructParameters {{"vs"}}.setGi(gi).setSpirv(pipeline_vert));
    auto fs       = Shader(Shader::ConstructParameters {{"fs"}, gi}.setSpirv(pipeline_frag));
    auto p        = GraphicsPipeline(GraphicsPipeline::ConstructParameters {}
                                         .setRenderPass(sw.renderPass())
                                         .setVS(&vs)
                                         .setFS(&fs)
                                         .dynamicScissor()
                                         .dynamicViewport()
                                         .addVertexAttribute(0, 0, vk::Format::eR32G32Sfloat)
                                         .addVertexBuffer(2 * sizeof(float)));

    // This part is what this sample is about. We create 2 uniform buffers and bind them to the pipeline via ArgumentPack.
    auto u0   = Buffer(Buffer::ConstructParameters {{"ub0"}, gi}.setUniform().setSize(sizeof(float) * 2));
    auto u1   = Buffer(Buffer::ConstructParameters {{"ub1"}, gi}.setUniform().setSize(sizeof(float) * 3));
    auto args = ArgumentPack({});
    args.b({0, 0}, {{u0.handle()}}).b({0, 1}, {{u1.handle()}});

    // We also need a vertex buffer to draw the triangle.
    auto bc = Buffer::SetContentParameters {}.setQueue(*device.graphics());
    auto vb = Buffer(Buffer::ConstructParameters {{"vb"}, gi}.setVertex().setSize(sizeof(float) * 2 * 3));
    vb.setContent(bc.setData<float>({-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f}));

    glfw.show();
    for (;;) {
        // Standard boilerplate of rendering a frame. It is basicaly the same as simple-triangle.cpp.
        if (options.headless) {
            if (sw.currentFrame().index > 10) break; // render 10 frames in headless mode.
            std::cout << "Frame " << sw.currentFrame().index << std::endl;
        } else {
            if (glfwWindowShouldClose(glfw.window)) break;
            glfwPollEvents();
        }
        auto & frame = sw.currentFrame();
        auto   c     = q.begin("pipeline");

        // Animate the triangle. Note that this is not the most efficient way to animate things, since it serializes
        // CPU and GPU. But it's simple and it is not the focus of this sample.
        auto elapsed = (float) frame.index / 60.0f;
        u0.setContent(bc.setData<float>({(float) std::sin(elapsed) * .25f, (float) std::cos(elapsed) * .25f}));
        u1.setContent(bc.setData<float>({(float) std::sin(elapsed) * .5f + .5f, (float) std::cos(elapsed) * .5f + .5f, 1.f}));

        // begin the render pass
        sw.cmdBeginBuiltInRenderPass(c, Swapchain::BeginRenderPassParameters {}.setColorF(0.0f, 1.0f, 0.0f, 1.0f)); // clear to green

        // bind the arguments to the pipeline
        p.cmdBind(c, args);

        // draw the triangle using the vertex buffer we created.
        p.cmdDraw(c, GraphicsPipeline::DrawParameters {}.setVertexBuffers({{vb.handle()}}).setNonIndexed(3));

        // end render pass
        sw.cmdEndBuiltInRenderPass(c);

        // submit and present
        q.submit({c, {}, {frame.imageAvailable}, {frame.renderFinished}});
        sw.present({});
    }
    device.waitIdle();
}

} // namespace pipeline

#ifndef UNIT_TEST
int main() { pipeline::entry({}); }
#endif