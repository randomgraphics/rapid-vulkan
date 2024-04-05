#include "test-instance.h"
#include "../3rd-party/catch2/catch.hpp"

using namespace rapid_vulkan;

TEST_CASE("api-version") {
    Instance instance({});
    CHECK(instance._cp.apiVersion >= vk::enumerateInstanceVersion(instance.dispatcher()));
}

TEST_CASE("instance-counter") {
    class A : public Root, public InstanceCounter<A> {
    public:
        A(): Root({}) {}
        ~A() override {}
    };

    Root * p = new A();
    REQUIRE(A::instanceCount() == 1);
    delete p;
    REQUIRE(A::instanceCount() == 0);
}

TEST_CASE("ref") {
    class A : public Root {
    public:
        A(): Root({}) {}
        ~A() override {}
    };

    class B : public A {
    public:
        B() {}
        ~B() override {}
    };

    SECTION("copy") {
        Ref<A> a1(new A());
        REQUIRE(a1->refCount() == 1);
        Ref<A> a2(a1);
        REQUIRE(a1->refCount() == 2);
        Ref<A> a3;
        a3 = a2;
        REQUIRE(a1->refCount() == 3);
    }

    SECTION("const-copy") {
        Ref<A> a1(new A());
        REQUIRE(a1->refCount() == 1);
        Ref<const A> a2(a1);
        REQUIRE(a1->refCount() == 2);
        Ref<const A> a3;
        a3 = a1;
        REQUIRE(a1->refCount() == 3);
    }

    SECTION("compatible-copy") {
        Ref<B> a1(new B());
        REQUIRE(a1->refCount() == 1);
        Ref<A> a2(a1);
        REQUIRE(a1->refCount() == 2);
        Ref<A> a3;
        a3 = a1;
        REQUIRE(a1->refCount() == 3);
    }

    SECTION("move") {
        Ref<A> a1(new A());
        REQUIRE(a1->refCount() == 1);
        Ref<A> a2(std::move(a1));
        REQUIRE(!a1);
        REQUIRE(a2->refCount() == 1);
        Ref<A> a3;
        a3 = std::move(a2);
        REQUIRE(!a2);
        REQUIRE(a3->refCount() == 1);
    }

    SECTION("const-move") {
        Ref<A> a1(new A());
        REQUIRE(a1->refCount() == 1);
        Ref<const A> a2(std::move(a1));
        REQUIRE(!a1);
        REQUIRE(a2->refCount() == 1);
        Ref<const A> a3;
        a3 = std::move(a2);
        REQUIRE(!a2);
        REQUIRE(a3->refCount() == 1);
    }

    SECTION("compatible-move") {
        Ref<B> a1(new B());
        REQUIRE(a1->refCount() == 1);
        Ref<A> a2(std::move(a1));
        REQUIRE(!a1);
        REQUIRE(a2->refCount() == 1);
        Ref<A> a3;
        a3 = std::move(a2);
        REQUIRE(!a2);
        REQUIRE(a3->refCount() == 1);
    }
};