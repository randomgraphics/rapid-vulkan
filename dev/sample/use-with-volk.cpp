#define VOLK_IMPLEMENTATION
#include <Volk/volk.h>

#ifndef NDEBUG
#define RAPID_VULKAN_ENABLE_DEBUG_BUILD 1
#endif
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan/rapid-vulkan.h>
using namespace RAPID_VULKAN_NAMESPACE;
int main() {
    volkInitialize();
    auto instance = Instance({});
    volkLoadInstance(instance);
    auto device = Device({instance});
    return 0;
}
