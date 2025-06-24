/*
    This is the most base form of graphics rendering using rapid-vulkan.
    It clears the screen using an animated color.
*/
#include "../rv.h"
#include <iostream>
#include <cmath>
#include <thread>

namespace clear_screen {

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
    vk::Instance                    inst      = VK_NULL_HANDLE;
    uint32_t                        headless  = 0; // Set to non-zero to enable headless mode. The value is number of frames to render.
    rapid_vulkan::Device::Verbosity verbosity = rapid_vulkan::Device::BRIEF;
};

void entry(const Options & options) {
    // Standard boilerplate of creating instance, device, swapchain, etc. It is basically the same as triangle.cpp.
    using namespace rapid_vulkan;
    auto                      instance = options.inst;
    std::unique_ptr<Instance> instancePtr;
    if (!instance) {
        instancePtr = std::make_unique<Instance>(Instance::ConstructParameters {}.setValidation(Instance::BREAK_ON_VK_ERROR).setBacktrace(backtrace));
        instance    = instancePtr->handle();
    }
    auto device = Device(Device::ConstructParameters {instance}.setPrintVkInfo(options.verbosity));
    auto gi     = device.gi();
    auto q      = CommandQueue({{"main"}, gi, device.graphics()->family(), device.graphics()->index()});
    auto w      = uint32_t(1280);
    auto h      = uint32_t(720);
    auto glfw   = GLFWInit(options.headless, instance, w, h, "pipeline-args");
    auto sw     = Swapchain(Swapchain::ConstructParameters {{"swapchain"}}
                                .setSurface(options.headless ? nullptr : glfw.surface)
                                .setDevice(device)
                                .setDimensions(options.headless ? w : 0, options.headless ? h : 0));
    // This part is what this sample is about. We create some buffers and bind them to the drawable.

    // show the window and begin the rendering loop.
    glfw.show();
    for (;;) {
        if (!options.headless && !glfw.processEvents()) break;
        auto  frame        = sw.beginFrame();
        float clearColor[] = {0.0f, 1.0f, 0.0f, 1.0f};
        if (frame) {
            // Standard boilerplate of rendering a frame. It is basically the same as triangle.cpp.
            if (options.headless) {
                if (frame->index > options.headless) break; // render required number of frames in headless mode, then quit.
                std::cout << "Frame " << frame->index << std::endl;
            }
            // Animate the clear color.
            auto elapsed  = (float) frame->index / 60.0f;
            clearColor[0] = (float) std::sin(elapsed) * 0.5f + 0.5f;
            clearColor[1] = (float) std::cos(elapsed * 1.5f) * 0.5f + 0.5f;
            clearColor[2] = (float) std::sin(elapsed * 2.0f) * 0.5f + 0.5f;

            // acquire a command buffer
            auto c = q.begin("pipeline");

            // begin the render pass
            sw.cmdBeginBuiltInRenderPass(c, Swapchain::BeginRenderPassParameters {}.setClearColorF(clearColor)); // clear the screen.

            // end render pass
            sw.cmdEndBuiltInRenderPass(c);

            // submit the command buffer
            q.submit({c, {}, {frame->imageAvailable}, {frame->renderFinished}});
        }

        // end of the frame.
        sw.present({});
    }
    device.waitIdle();
}

} // namespace clear_screen

#ifndef UNIT_TEST
int main() { clear_screen::entry({}); }
#endif