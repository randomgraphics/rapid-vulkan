#include "../3rd-party/catch2/catch.hpp"
#include "test-instance.h"

#define UNIT_TEST

#ifndef __ANDROID__

#include "../sample/triangle.cpp"
TEST_CASE("sample-triangle", "[sample]") {
    triangle::Options o;
    o.headless = 10; // render 10 frames.

    SECTION("dynamic viewports") {
        o.inst            = TestVulkanInstance::instance->handle();
        o.dynamicViewport = true;
        o.verbosity       = rapid_vulkan::Device::SILENCE;
        triangle::entry(o);
    }

    SECTION("static viewports") {
        o.inst            = TestVulkanInstance::instance->handle();
        o.dynamicViewport = false;
        o.verbosity       = rapid_vulkan::Device::SILENCE;
        triangle::entry(o);
    }
}

#include "../sample/drawable.cpp"
TEST_CASE("sample-drawable", "[sample]") {
    drawable::Options o;
    o.headless  = 10; // render 10 frames.
    o.inst      = TestVulkanInstance::instance->handle();
    o.verbosity = rapid_vulkan::Device::SILENCE;
    drawable::entry(o);
}

#endif