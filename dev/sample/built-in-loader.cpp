#ifndef NDEBUG
    #define RAPID_VULKAN_ENABLE_DEBUG_BUILD 1
#endif
#define RAPID_VULKAN_IMPLEMENTATION
#define RAPID_VULKAN_ENABLE_LOADER 1
#include <rapid-vulkan/rapid-vulkan.h>
using namespace RAPID_VULKAN_NAMESPACE;
int main() {
    auto instance = Instance({});
    auto device   = Device({instance});
    return 0;
}
