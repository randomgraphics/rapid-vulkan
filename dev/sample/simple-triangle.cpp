#include "../rv.h"
#include <iostream>

namespace simple_triangle {

#include "shader/simple-triangle.vert.spv.h"
#include "shader/simple-triangle.frag.spv.h"

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
    vk::Instance                    inst            = VK_NULL_HANDLE;
    bool                            headless        = false;
    bool                            dynamicViewport = true;
    rapid_vulkan::Device::Verbosity verbosity       = rapid_vulkan::Device::BRIEF;
};

void entry(const Options & options) {
    using namespace rapid_vulkan;
    auto                      instance = options.inst;
    std::unique_ptr<Instance> instancePtr;
    if (!instance) {
        instancePtr = std::make_unique<Instance>(Instance::ConstructParameters {}.setValidation(Instance::BREAK_ON_VK_ERROR));
        instance    = instancePtr->handle();
    }
    auto w      = uint32_t(1280);
    auto h      = uint32_t(720);
    auto glfw   = GLFWInit(options.headless, instance, w, h, "simple-triangle");
    auto device = Device(Device::ConstructParameters {instance}.setSurface(glfw.surface).setPrintVkInfo(options.verbosity));
    auto gi     = device.gi();
    auto vs     = Shader(Shader::ConstructParameters {{"simple-triangle-vs"}}.setGi(gi).setSpirv(simple_triangle_vert));
    auto fs     = Shader(Shader::ConstructParameters {{"simple-triangle-fs"}, gi}.setSpirv(simple_triangle_frag));
    auto q      = CommandQueue({{"main"}, gi, device.graphics()->family(), device.graphics()->index()});
    auto sw     = Swapchain(Swapchain::ConstructParameters {{"simple-triangle"}}.setDevice(device).setDimensions(w, h));

    // create the graphics pipeline
    auto gcp = GraphicsPipeline::ConstructParameters {{"simple-triangle"}}.setRenderPass(sw.renderPass()).setVS(&vs).setFS(&fs);
    gcp.rast.setCullMode(vk::CullModeFlagBits::eNone);
    if (options.dynamicViewport) {
        // Dynamic viewport and scissor. When window surface is resized, the viewport and scissor will be updated automatically w/o recreating the pipeline.
        gcp.dynamicScissor().dynamicViewport();
    } else {
        // When using static viewport and scissor, the viewport won't change along with the window surface.g st
        gcp.viewports.push_back(vk::Viewport(0.f, 0.f, (float) w, (float) h, 0.f, 1.f));
        gcp.scissors.push_back(vk::Rect2D({0, 0}, {w, h}));
    }
    auto p = GraphicsPipeline(gcp);

    glfw.show();
    for (;;) {
        if (options.headless) {
            if (sw.currentFrame().index > 10) break; // render 10 frames in headless mode.
            std::cout << "Frame " << sw.currentFrame().index << std::endl;
        } else {
            if (glfwWindowShouldClose(glfw.window)) break;
            glfwPollEvents();
        }
        auto & frame = sw.currentFrame();
        auto   c     = q.begin("simple-triangle");
        sw.cmdBeginBuiltInRenderPass(c, Swapchain::BeginRenderPassParameters {}.setClearColorF({0.0f, 1.0f, 0.0f, 1.0f})); // clear to green
        p.cmdDraw(c, GraphicsPipeline::DrawParameters {}.setNonIndexed(3));                                                // then draw a blue triangle.
        sw.cmdEndBuiltInRenderPass(c);
        q.submit({c, {}, {frame.imageAvailable}, {frame.renderFinished}});
        sw.present({});
    }
    device.waitIdle(); // don't forget to wait for the device to be idle before destroying vulkan objects.
}

} // namespace simple_triangle

#ifndef UNIT_TEST
int main() { simple_triangle::entry({}); }
#endif