#define VOLK_IMPLEMENTATION
#include <Volk/volk.h>

#define RAPID_VULKAN_ENABLE_LOADER 0
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan/rapid-vulkan.h>
using namespace rapid_vulkan;
int main() {
    volkInitialize();
    auto instance = Instance({});
    volkLoadInstance(instance);
    auto device = Device(instance);
    return 0;
}
