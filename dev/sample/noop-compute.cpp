#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan/rapid-vulkan.h>
#include "shader/noop.comp.spv.h"
int main() {
    using namespace rapid_vulkan;
    auto instance = Instance({});
    auto device   = Device(instance.dcp());
    auto gi       = device.gi();
    auto noop     = Shader({{"noop"}, gi, {sizeof(noop_comp) / sizeof(uint32_t), (const uint32_t *) noop_comp}});
    auto p        = ComputePipeline({{"noop"}, noop});
    auto q        = CommandQueue({{"main"}, gi, device.graphics()->family(), device.graphics()->index()});
    if (auto c = q.begin("main")) {
        p.cmdDispatch(c, {1, 1, 1});
        q.submit(c);
    }
    q.wait();
    return 0;
}
