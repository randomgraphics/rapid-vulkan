#include "../3rd-party/catch2/catch.hpp"

#define UNIT_TEST

#ifndef __ANDROID__
#include "../sample/simple-triangle.cpp"
TEST_CASE("simple-triangle", "[sample]") {
    simple_triangle::Options o;
    o.headless = true;

    SECTION("dynamic viewports") {
        o.dynamicViewport = true;
        simple_triangle::entry(o);
    }

    SECTION("static viewports") {
        o.dynamicViewport = false;
        simple_triangle::entry(o);
    }
}
#endif