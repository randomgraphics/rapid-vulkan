#include "test-instance.h"
#include "../3rd-party/catch2/catch2.hpp"

TEST_CASE("api-version") {
    using namespace rapid_vulkan;
    Instance instance({});
    CHECK(instance.cp().apiVersion >= vk::enumerateInstanceVersion());
}