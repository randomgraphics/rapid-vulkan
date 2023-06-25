#define VOLK_IMPLEMENTATION
#ifdef _WIN32
#include <Volk/volk.h>
#else
#include <volk.h>
#endif

#define RAPID_VULKAN_ENABLE_LOADER 0
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan/rapid-vulkan.h>
using namespace rapid_vulkan;
int main() {
    volkInitialize();
    auto instance = Instance({});
    volkLoadInstance(instance);
    auto device = Device({instance.handle()});
    return 0;
}
