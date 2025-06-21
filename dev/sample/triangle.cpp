/* This is a very basic demonstration of the Pipeline object in rapid-vulakn.
 */
#include "../rv.h"
#include <iostream>
#include <thread>

namespace triangle {

#include "shader/triangle.vert.spv.h"
#include "shader/triangle.frag.spv.h"

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
    bool processEvents() const {
        if (!window) return false;
        glfwPollEvents();
        if (glfwWindowShouldClose(window)) return false;
        while (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            glfwPollEvents();
        }
        return true;
    }
};

struct Options {
    vk::Instance                    inst            = VK_NULL_HANDLE;
    uint32_t                        headless        = 0; // Set to non-zero to enable headless mode. The value is number of frames to render.
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
    auto device = Device(Device::ConstructParameters {instance}.setPrintVkInfo(options.verbosity));
    auto gi     = device.gi();
    auto vs     = Shader(Shader::ConstructParameters {{"triangle-vs"}}.setGi(gi).setSpirv(triangle_vert));
    auto fs     = Shader(Shader::ConstructParameters {{"triangle-fs"}, gi}.setSpirv(triangle_frag));
    auto q      = CommandQueue({{"main"}, gi, device.graphics()->family(), device.graphics()->index()});

    // create the swapchain.
    auto w    = uint32_t(1280);
    auto h    = uint32_t(720);
    auto glfw = GLFWInit(options.headless, instance, w, h, "triangle");
    auto sw   = Swapchain(Swapchain::ConstructParameters {{"triangle"}}
                              .setSurface(options.headless ? nullptr : glfw.surface)
                              .setDevice(device)
                              .setDimensions(options.headless ? w : 0, options.headless ? h : 0));

    // create the graphics pipeline
    auto gcp = GraphicsPipeline::ConstructParameters {{"triangle"}}.setRenderPass(sw.renderPass()).setVS(&vs).setFS(&fs);
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
        if (!options.headless && !glfw.processEvents()) break;
        auto frame = sw.beginFrame();
        if (frame) {
            if (options.headless) {
                if (frame->index > options.headless) break; // only render number of required frames in headless mode, then quite.
                std::cout << "Frame " << frame->index << std::endl;
            }
            auto c = q.begin("triangle");
            sw.cmdBeginBuiltInRenderPass(c, Swapchain::BeginRenderPassParameters {}.setClearColorF({0.0f, 1.0f, 0.0f, 1.0f})); // clear to green
            p.cmdDraw(c, GraphicsPipeline::DrawParameters {}.setNonIndexed(3));                                                // then draw a blue triangle.
            sw.cmdEndBuiltInRenderPass(c);
            q.submit({c, {}, {frame->imageAvailable}, {frame->renderFinished}});
        }
        sw.present({});
    }
    device.waitIdle();
}

} // namespace triangle

#ifndef UNIT_TEST
int main() { triangle::entry({}); }
#endif