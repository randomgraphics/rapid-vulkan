#include <rapid-vulkan/rapid-vulkan.cpp>
int main() {
    auto instance = rapid_vulkan::Instance({});
    auto device   = rapid_vulkan::Device(instance.dcp());
    return 0;
}
