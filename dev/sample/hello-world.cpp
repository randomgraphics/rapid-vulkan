#include "../rv.h"
int main() {
    auto instance = rapid_vulkan::Instance({});
    auto device   = rapid_vulkan::Device(instance);
    return 0;
}
