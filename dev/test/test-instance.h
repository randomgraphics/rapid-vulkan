#include <rapid-vulkan/rapid-vulkan.h>
#include <memory>

struct TestVulkanInstance {
    inline static std::unique_ptr<rapid_vulkan::Instance> instance;
    inline static std::unique_ptr<rapid_vulkan::Device>   device;
};
