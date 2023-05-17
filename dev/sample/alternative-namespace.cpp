#define RAPID_VULKAN_NAMESPACE test_namespace
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan/rapid-vulkan.h>

void main() {
    auto instance = test_namespace::Instance({});
    auto device   = test_namespace::Device(instance.dcp());
}