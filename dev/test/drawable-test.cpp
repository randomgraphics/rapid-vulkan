#include "../3rd-party/catch2/catch.hpp"
#include "test-instance.h"

/// Make sure calling hibernate multiple times on command buffers is safe.
TEST_CASE("drawable-duplicated-command-buffers") {
    auto q = TestVulkanInstance::device->graphics()->clone();
    auto c = q.begin(nullptr); // testing null name is safe
    q.submit({{c, c}});
    q.submit({{c, c}});
}

TEST_CASE("drawable-wait-idle") {
    auto q = TestVulkanInstance::device->graphics()->clone();
    auto c = q.begin(nullptr); // testing null name is safe
    auto s = q.submit({c});
    q.waitIdle(); // wait for the queue to be idle.
    q.wait(s);    // wait on an already finished submission is safe and not an error.
}

TEST_CASE("drawable-reuse") {
    auto q = TestVulkanInstance::device->graphics();

    SECTION("reuse-finished") {
        auto c1 = q->begin("c1");
        auto h1 = c1.handle();
        q->wait(q->submit({c1}));
        auto c2 = q->begin("c2"); // should reuse c1
        auto h2 = c2.handle();
        CHECK(h1 == h2); // make sure the handles are the same.
    }

    SECTION("reuse-dropped") {
        auto c1 = q->begin("c1");
        auto h1 = c1.handle();
        q->drop(c1);
        auto c2 = q->begin("c2"); // should reuse c1
        auto h2 = c2.handle();
        CHECK(h1 == h2); // make sure the handles are the same.
    }

    SECTION("should-not-reuse-active") {
        auto c1 = q->begin("c1");
        auto h1 = c1.handle();
        auto c2 = q->begin("c2"); // should not reuse c1
        auto h2 = c2.handle();
        CHECK(h1 != h2);
    }

    SECTION("should-not-reuse-pending") {
        auto c1 = q->begin("c1");
        auto h1 = c1.handle();
        q->submit({c1});
        auto c2 = q->begin("c2"); // should not reuse c1
        auto h2 = c2.handle();
        CHECK(h1 != h2);
    }
}