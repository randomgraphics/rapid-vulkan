#include "../rv.h"
#include <iostream>
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
        window = glfwCreateWindow((int)w, (int)h, title, nullptr, nullptr);
        RVI_REQUIRE(window, "Failed to create GLFW window.");
        glfwCreateWindowSurface(instance, window, nullptr, &surface);
        RVI_REQUIRE(surface, "Failed to create surface.");
    }
    ~GLFWInit() {
        if (surface) inst.destroySurfaceKHR(surface), surface = VK_NULL_HANDLE;
        if (window) glfwDestroyWindow(window), window = nullptr;
        glfwTerminate();
    }
};

void entry(bool headless) {
    using namespace rapid_vulkan;
    auto w        = uint32_t(1280);
    auto h        = uint32_t(720);
    auto instance = Instance(Instance::ConstructParameters {}.setValidation(Instance::BREAK_ON_VK_ERROR));
    auto glfw     = GLFWInit(headless, instance, w, h, "simple-triangle");
    auto device   = Device(instance.dcp().setSurface(glfw.surface));
    auto gi       = device.gi();
    auto vs       = Shader(Shader::ConstructParameters {{"simple-triangle-vs"}}.setGi(gi).setSpirv(simple_triangle_vert));
    auto fs       = Shader(Shader::ConstructParameters {{"simple-triangle-fs"}, gi}.setSpirv(simple_triangle_frag));
    auto q        = CommandQueue({{"main"}, gi, device.graphics()->family(), device.graphics()->index()});
    auto sw       = Swapchain(Swapchain::ConstructParameters {{"simple-triangle"}}.setDevice(device).setDimensions(w, h));

    // create the graphics pipeline
    auto gcp = GraphicsPipeline::ConstructParameters {{"simple-triangle"}}.setRenderPass(sw.renderPass()).setVS(&vs).setFS(&fs);
    gcp.rast.setCullMode(vk::CullModeFlagBits::eNone);
#if 1
    // Dynamic viewport and scissor. When window surface is resized, the viewport and scissor will be updated automatically w/o recreating the pipeline.
    gcp.dynamicScissor().dynamicViewport();
#else
    // When using static viewport and scissor, the viewport won't change along with the window surface.g st
    gcp.viewports.push_back(vk::Viewport(0.f, 0.f, (float) w, (float) h, 0.f, 1.f));
    gcp.scissors.push_back(vk::Rect2D({0, 0}, {w, h}));
#endif
    auto p = GraphicsPipeline(gcp);

    // Endless render loop for this simple sample.
    if (!headless) glfwShowWindow(glfw.window);
    for(;;) {
        if (headless) {
            if (sw.currentFrame().index > 10) break; // render 10 frames in headless mode.
            std::cout << "Frame " << sw.currentFrame().index << std::endl;
        } else {
            if (glfwWindowShouldClose(glfw.window)) break;
            glfwPollEvents();
        }
        auto & frame = sw.currentFrame();
        auto   c     = q.begin("simple-triangle");
        sw.cmdBeginBuiltInRenderPass(c, Swapchain::BeginRenderPassParameters {}.setColorF(0.0f, 1.0f, 0.0f, 1.0f)); // clear to green
        p.cmdDraw(c, GraphicsPipeline::DrawParameters {}.setNonIndexed(3));                                         // then draw a blue triangle.
        sw.cmdEndBuiltInRenderPass(c);
        q.submit({c, {}, {frame.imageAvailable}, {frame.renderFinished}});
        sw.present({});
    }
    device.waitIdle();
}

int main() {
    entry(true);
}