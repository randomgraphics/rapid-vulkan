#include "../3rd-party/catch2/catch.hpp"
#include "test-instance.h"

#ifndef __ANDROID__
#include <atomic>

namespace {
struct GLFWSession {
    ~GLFWSession() { glfwTerminate(); }
};

VKAPI_ATTR VkBool32 VKAPI_CALL countValidationErrors(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *, void * data) {
    ++*static_cast<std::atomic_uint *>(data);
    return VK_FALSE;
}
} // namespace

// Explicitly selected by CI under Xvfb: ordinary headless CIT needs no display.
TEST_CASE("window-swapchain-acquisition", "[.window]") {
    using namespace rapid_vulkan;
    GLFWSession glfw;
    REQUIRE(glfwInit() == GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto window = std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)>(glfwCreateWindow(128, 72, "swapchain test", nullptr, nullptr), glfwDestroyWindow);
    REQUIRE(window);

    auto cp = Instance::ConstructParameters {}.setValidation(Instance::LOG_ON_VK_ERROR);
    // Validation is normally optional; this regression test must never silently disable it.
    cp.layers.emplace_back("VK_LAYER_KHRONOS_validation", true);
    cp.instanceExtensions[VK_EXT_DEBUG_UTILS_EXTENSION_NAME] = true;
    uint32_t extensionCount                                  = 0;
    auto     extensions                                      = glfwGetRequiredInstanceExtensions(&extensionCount);
    REQUIRE(extensions);
    cp.addExtensions(true, extensions, extensionCount);
    auto                               instance = Instance(cp);
    std::atomic_uint                   errors {0};
    VkDebugUtilsMessengerCreateInfoEXT debugInfo {};
    debugInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback = countValidationErrors;
    debugInfo.pUserData       = &errors;
    auto messenger            = instance.handle().createDebugUtilsMessengerEXTUnique(vk::DebugUtilsMessengerCreateInfoEXT(debugInfo));
    {
        VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
        REQUIRE(glfwCreateWindowSurface(instance, window.get(), nullptr, &rawSurface) == VK_SUCCESS);
        auto surface = vk::UniqueSurfaceKHR(rawSurface, {instance.handle()});
        auto device  = Device(Device::ConstructParameters {instance}.setPrintVkInfo(Device::SILENCE));
        bool render  = false;
        SECTION("present without rendering") {}
        SECTION("built-in render pass") { render = true; }

        // Rebuilding on the same surface exercises initialization again after presentation.
        for (unsigned generation = 0; generation < 2; ++generation) {
            auto swapchain = Swapchain(Swapchain::ConstructParameters {{"window test"}}.setDevice(device).setSurface(surface.get()));
            REQUIRE(errors.load() == 0);
            auto & queue    = swapchain.graphics();
            auto   rendered = device.gi()->device.createSemaphoreUnique({});
            for (unsigned i = 0; i < 4; ++i) {
                glfwPollEvents();
                auto frame = swapchain.beginFrame();
                REQUIRE(frame.valid());
                if (i == 0) {
                    auto state = frame.backbuffer->image->getState().get(0, 0, vk::ImageAspectFlagBits::eColor);
                    REQUIRE(state);
                    // Also detects eager initialization on older validation-layer versions.
                    CHECK(state->layout == vk::ImageLayout::eUndefined);
                }
                auto wait = frame.imageAvailable;
                if (render) {
                    auto commands = queue.begin("clear acquired image");
                    swapchain.cmdBeginBuiltInRenderPass(commands, {});
                    swapchain.cmdEndBuiltInRenderPass(commands);
                    CommandQueue::SyncPoint acquired {frame.imageAvailable};
                    CommandQueue::SyncPoint finished {rendered.get()};
                    queue.submit1({commands, {}, {1, &acquired}, {}, {1, &finished}});
                    wait = rendered.get();
                }
                REQUIRE(swapchain.present(Swapchain::PresentParameters {}.setRenderFinished({wait})));
                // Keep semaphore reuse independent of presentation scheduling in this small test.
                device.waitIdle();
                REQUIRE(errors.load() == 0);
            }
        }
    }
    CHECK(errors.load() == 0);
}
#endif
