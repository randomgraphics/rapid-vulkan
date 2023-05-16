#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#define RAPID_VULKAN_LOG_ERROR(...)                                                                          \
    do {                                                                                                     \
        auto message___ = rapid_vulkan::format("[  ERROR] %s\n", rapid_vulkan::format(__VA_ARGS__).c_str()); \
        fprintf(stderr, message___.c_str());                                                                 \
        ::OutputDebugStringA(message___.c_str());                                                            \
    } while (false)
#define RAPID_VULKAN_LOG_WARNING(...)                                                                        \
    do {                                                                                                     \
        auto message___ = rapid_vulkan::format("[WARNING] %s\n", rapid_vulkan::format(__VA_ARGS__).c_str()); \
        fprintf(stderr, message___.c_str());                                                                 \
        ::OutputDebugStringA(message___.c_str());                                                            \
    } while (false)
#endif
#include <rapid-vulkan/rapid-vulkan.h>
#include <memory>

struct TestVulkanInstance {
    inline static std::unique_ptr<rapid_vulkan::Instance> instance;
    inline static std::unique_ptr<rapid_vulkan::Device>   device;
};
