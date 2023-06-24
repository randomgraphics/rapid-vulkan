#include "../3rd-party/catch2/catch.hpp"
#include "test-instance.h"

using namespace rapid_vulkan;

TEST_CASE("drawable-default-construction") {
    auto d = Drawable({});
    d.compile();
}