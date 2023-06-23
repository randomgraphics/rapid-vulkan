#include "test-instance.h"
#include "../3rd-party/catch2/catch.hpp"

using namespace rapid_vulkan;

TEST_CASE("api-version") {
    Instance instance({});
    CHECK(instance.cp().apiVersion >= vk::enumerateInstanceVersion());
}

TEST_CASE("ref") {
    class A : public Root {
    public:
        A(): Root({}) {}
        virtual ~A() {}
    };

    Ref<A> a1(new A());
    REQUIRE(a1->refCount() == 1);
    Ref<A> a2(a1);
    REQUIRE(a1->refCount() == 2);
};