#define RAPID_VULKAN_IMPLEMENTATION
#define RAPID_VULKAN_ENABLE_LOADER 1
#include <rapid-vulkan/rapid-vulkan.h>
#include "shader/noop.comp.spv.h"
int main() {
    auto instance = rapid_vulkan::Instance({});
    auto device   = rapid_vulkan::Device({instance});
    auto gi       = device.gi();
    auto noop     = rapid_vulkan::Shader({"noop", gi, {sizeof(noop_comp) / sizeof(uint32_t), (const uint32_t *) noop_comp}});
    auto p        = rapid_vulkan::ComputePipeline({"noop", noop});
    auto q        = rapid_vulkan::CommandQueue({"main", gi, device.graphics()->family(), device.graphics()->index()});
    if (auto c = q.begin("main")) {
        p.dispatch(c->handle(), {1, 1, 1});
        q.submit(c);
    }
    q.wait();
    return 0;
}
