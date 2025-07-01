/*
    This is a slightly more complex sample than the triangle sample. It demonstrates the basic usage of the Drawable class.
*/
#include "../rv.h"
#include <iostream>
#include <cmath>
#include <thread>

using namespace rapid_vulkan;
struct RenderBase {
    Ref<Instance> instance;
    Ref<Device>   device;
    Ref<CommandQueue> commandQueue;
    uint32_t width = 1280;
    uint32_t height = 720;
    GLFWwindow * window = nullptr;
    VkSurfaceKHR surface = nullptr;
    Ref<Swapchain> swapchain;

    RenderBase(bool headless, uint32_t w, uint32_t h, const char * title) {
        instance = Ref<Instance>::make(Instance::ConstructParameters {}.setValidation(Instance::BREAK_ON_VK_ERROR).setBacktrace(backtrace));
        device = Ref<Device>::make(Device::ConstructParameters {instance}.setPrintVkInfo(Device::BRIEF));
        commandQueue = Ref<CommandQueue>::make(CommandQueue::ConstructParameters {{"main"}, device->gi(), device->graphics()->family(), device->graphics()->index()});
        width = w;
        height = h;
        if (!headless) {
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            window = glfwCreateWindow((int) w, (int) h, title, nullptr, nullptr);
            RVI_REQUIRE(window, "Failed to create GLFW window.");
            glfwCreateWindowSurface(instance, window, nullptr, &surface);
            RVI_REQUIRE(surface, "Failed to create surface.");
        }
        swapchain = Ref<Swapchain>::make(Swapchain::ConstructParameters {{"swapchain"}}
                                .setSurface(headless ? nullptr : surface)
                                .setDevice(device)
                                .setDimensions(headless ? w : 0, options.headless ? h : 0));
    }
 
    ~RenderBase() {
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
