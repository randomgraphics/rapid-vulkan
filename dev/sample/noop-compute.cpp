#include "../rv.h"
#include "shader/noop.comp.spv.h"
int main() {
    using namespace rapid_vulkan;
    auto instance = Instance({});
    auto device   = Device(instance.dcp());
    auto gi       = device.gi();
    auto noop     = Shader(Shader::ConstructParameters {{"noop"}, gi}.setSpirv(noop_comp));
    auto p        = ComputePipeline({{"noop"}, &noop});
    auto q        = CommandQueue({{"main"}, gi, device.graphics()->family(), device.graphics()->index()});
    if (auto c = q.begin("main")) {
        p.cmdDispatch(c, {1, 1, 1});
        q.submit({c});
    }
    q.waitIdle();
    return 0;
}
