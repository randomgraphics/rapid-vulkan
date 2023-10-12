#include "../3rd-party/catch2/catch.hpp"
#include "test-instance.h"

#define UNIT_TEST

#ifndef __ANDROID__
#include "../sample/simple-triangle.cpp"
TEST_CASE("simple-triangle", "[sample]") {
    simple_triangle::Options o;
    o.headless = 1;

    SECTION("dynamic viewports") {
        o.inst            = TestVulkanInstance::instance->handle();
        o.dynamicViewport = true;
        o.verbosity       = rapid_vulkan::Device::SILENCE;
        simple_triangle::entry(o);
    }

    SECTION("static viewports") {
        o.inst            = TestVulkanInstance::instance->handle();
        o.dynamicViewport = false;
        o.verbosity       = rapid_vulkan::Device::SILENCE;
        simple_triangle::entry(o);
    }
}
#endif