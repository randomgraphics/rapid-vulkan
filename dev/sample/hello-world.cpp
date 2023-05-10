#define RAPID_VULKAN_IMPLEMENTATION
#define RAPID_VULKAN_ENABLE_LOADER 1
#include <rapid-vulkan/rapid-vulkan.h>
int main() {
    auto instance = rapid_vulkan::Instance({});
    auto device   = rapid_vulkan::Device({instance});
    return 0;
}
