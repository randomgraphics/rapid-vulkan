#include "../3rd-party/catch2/catch.hpp"
#include "test-instance.h"

using namespace rapid_vulkan;

class DummyPipeline : public Pipeline {
public:
    static Ref<const Pipeline> g() {
        auto p = new DummyPipeline(vk::PipelineBindPoint::eGraphics);
        return Ref<const Pipeline>(p);
    }

    static Ref<const Pipeline> c() {
        auto p = new DummyPipeline(vk::PipelineBindPoint::eCompute);
        return Ref<const Pipeline>(p);
    }

private:
    DummyPipeline(vk::PipelineBindPoint b): Pipeline("dummy", b, {}) {}
};

TEST_CASE("drawable-default") {
    auto d = Drawable({});
    d.compile();
}

TEST_CASE("drawable-dummy") {
    auto d = Drawable({{}, DummyPipeline::g()});
    d.compile();
}

TEST_CASE("drawable-smoke") {
    auto d = Drawable({{}, DummyPipeline::c()});

    d.dispatch({1, 2, 3});
    auto p1 = d.compile();

    // modification of drawable should not affect already-compiled draw pack. So p1's dispatch parameters should still be {1, 2, 3}
    d.dispatch({4, 5, 6});
    auto p2 = d.compile();

    REQUIRE(p1->dispatch.width == 1);
    REQUIRE(p1->dispatch.height == 2);
    REQUIRE(p1->dispatch.depth == 3);

    REQUIRE(p2->dispatch.width == 4);
    REQUIRE(p2->dispatch.height == 5);
    REQUIRE(p2->dispatch.depth == 6);
}