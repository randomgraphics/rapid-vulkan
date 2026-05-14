#include "../3rd-party/catch2/catch.hpp"
#include "test-instance.h"

/// Make sure calling hibernate multiple times on command buffers is safe.
TEST_CASE("queue-duplicated-command-buffers") {
    auto q = TestVulkanInstance::device->graphics()->clone();
    auto c = q.begin(nullptr); // testing null name is safe
    q.submit1({{c, c}});
    q.submit1({{c, c}});
}

TEST_CASE("queue-wait-idle") {
    auto q = TestVulkanInstance::device->graphics()->clone();
    auto c = q.begin(nullptr); // testing null name is safe
    auto s = q.submit1({c});
    q.waitIdle(); // wait for the queue to be idle.
    q.wait(s);    // wait on an already finished submission is safe and not an error.
}

TEST_CASE("queue-reuse") {
    auto q = TestVulkanInstance::device->graphics();

    SECTION("reuse-finished") {
        auto c1 = q->begin("c1");
        auto h1 = c1.handle();
        q->wait(q->submit1({c1}));
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
        q->submit1({c1});
        auto c2 = q->begin("c2"); // should not reuse c1
        auto h2 = c2.handle();
        CHECK(h1 != h2);
    }
}

/// Verify submit1 handles binary semaphore wait + signal correctly.
/// One submit signals a binary semaphore; the next waits on it.
TEST_CASE("queue-submit1-binary-semaphore") {
    using namespace rapid_vulkan;
    auto q  = TestVulkanInstance::device->graphics();
    auto gi = TestVulkanInstance::device->gi();

    auto sem = gi->device.createSemaphoreUnique({}, gi->allocator);

    CommandQueue::SyncPoint signalSp {sem.get()};
    auto                    c1 = q->begin("signal");
    q->submit1({c1, {}, {}, {}, {1, &signalSp}});

    CommandQueue::SyncPoint waitSp {sem.get()};
    auto                    c2 = q->begin("wait");
    q->submit1({c2, {}, {1, &waitSp}});

    q->waitIdle();
    CHECK(true); // reaching here means no GPU hang / deadlock
}

/// Verify submit2 handles timeline semaphore signal + wait via VkTimelineSemaphoreSubmitInfo.
TEST_CASE("queue-submit1-timeline-semaphore") {
    using namespace rapid_vulkan;
    auto q  = TestVulkanInstance::device->graphics();
    auto gi = TestVulkanInstance::device->gi();

    if (!TestVulkanInstance::timelineSemaphore) {
        fprintf(stderr, "timeline semaphore not available, skipping.\n");
        return;
    }

    vk::SemaphoreTypeCreateInfo timelineInfo {vk::SemaphoreType::eTimeline, 0};
    vk::SemaphoreCreateInfo     semInfo {};
    semInfo.setPNext(&timelineInfo);
    auto timeline = gi->device.createSemaphoreUnique(semInfo, gi->allocator);

    CommandQueue::SyncPoint signalPt {timeline.get(), 1};
    auto                    c1 = q->begin("timeline-signal");
    q->submit2({c1, {}, {}, {}, {}, {1, &signalPt}});

    CommandQueue::SyncPoint waitPt {timeline.get(), 1};
    auto                    c2 = q->begin("timeline-wait");
    q->submit2({c2, {}, {}, {1, &waitPt}});

    q->waitIdle();
    CHECK(true); // reaching here means no GPU hang / deadlock
}

/// Verify submit2 handles binary semaphore wait + signal via VkSubmitInfo2.
TEST_CASE("queue-submit2-binary-semaphore") {
    using namespace rapid_vulkan;
    auto q  = TestVulkanInstance::device->graphics();
    auto gi = TestVulkanInstance::device->gi();

    if (!TestVulkanInstance::synchronization2) {
        fprintf(stderr, "synchronization2 not available, skipping.\n");
        return;
    }

    auto sem = gi->device.createSemaphoreUnique({}, gi->allocator);

    CommandQueue::SyncPoint signalSp {sem.get()};
    auto                    c1 = q->begin("signal2");
    q->submit2({c1, {}, {}, {}, {1, &signalSp}});

    CommandQueue::SyncPoint waitSp {sem.get()};
    auto                    c2 = q->begin("wait2");
    q->submit2({c2, {}, {1, &waitSp}});

    q->waitIdle();
    CHECK(true); // reaching here means no GPU hang / deadlock
}

/// A truly empty submission (no commands, no semaphores) must return an empty SubmissionID.
TEST_CASE("queue-submit-truly-empty-is-ignored") {
    using namespace rapid_vulkan;
    auto q = TestVulkanInstance::device->graphics()->clone();

    auto id1 = q.submit1({});
    auto id2 = q.submit2({});
    CHECK(id1.empty());
    CHECK(id2.empty());
}

/// Bridge-only submission (no command buffer, but with timeline-wait + binary-signal) must succeed.
/// This pattern is needed to bridge a timeline sync point to a binary semaphore for vkQueuePresentKHR.
TEST_CASE("queue-submit2-bridge-only") {
    using namespace rapid_vulkan;
    auto q  = TestVulkanInstance::device->graphics()->clone();
    auto gi = TestVulkanInstance::device->gi();

    if (!TestVulkanInstance::synchronization2 || !TestVulkanInstance::timelineSemaphore) {
        fprintf(stderr, "synchronization2 or timelineSemaphore not available, skipping.\n");
        return;
    }

    // Create a timeline semaphore and a binary semaphore.
    vk::SemaphoreTypeCreateInfo timelineInfo {vk::SemaphoreType::eTimeline, 0};
    vk::SemaphoreCreateInfo     semInfo {};
    semInfo.setPNext(&timelineInfo);
    auto timeline = gi->device.createSemaphoreUnique(semInfo, gi->allocator);
    auto binary   = gi->device.createSemaphoreUnique({}, gi->allocator);

    // Submit some work that signals the timeline point.
    CommandQueue::SyncPoint signalPt {timeline.get(), 1, vk::PipelineStageFlagBits::eAllCommands};
    auto                    c1 = q.begin("work");
    q.submit2({c1, {}, {}, {}, {}, {1, &signalPt}});

    // Bridge-only: wait on timeline point, signal binary — no command buffer.
    CommandQueue::SyncPoint waitPt    = {timeline.get(), 1, vk::PipelineStageFlagBits::eAllCommands};
    CommandQueue::SyncPoint signalBin = {binary.get(), 0, vk::PipelineStageFlagBits::eAllCommands};
    auto                    bridgeId  = q.submit2({{}, {}, {}, {1, &waitPt}, {1, &signalBin}});
    CHECK(!bridgeId.empty()); // bridge submit must produce a trackable submission

    // Consumer: wait on the binary semaphore to confirm the bridge actually ran.
    CommandQueue::SyncPoint waitBin = {binary.get(), 0, vk::PipelineStageFlagBits::eAllCommands};
    auto                    c2      = q.begin("consumer");
    q.submit2({c2, {}, {1, &waitBin}});

    q.waitIdle();
    CHECK(true); // reaching here without a hang confirms the bridge semaphore was signaled
}

/// Verify submit2 handles timeline semaphore signal + wait via VkSubmitInfo2.
TEST_CASE("queue-submit2-timeline-semaphore") {
    using namespace rapid_vulkan;
    auto q  = TestVulkanInstance::device->graphics();
    auto gi = TestVulkanInstance::device->gi();

    if (!TestVulkanInstance::synchronization2 || !TestVulkanInstance::timelineSemaphore) {
        fprintf(stderr, "synchronization2 or timelineSemaphore not available, skipping.\n");
        return;
    }

    vk::SemaphoreTypeCreateInfo timelineInfo {vk::SemaphoreType::eTimeline, 0};
    vk::SemaphoreCreateInfo     semInfo {};
    semInfo.setPNext(&timelineInfo);
    auto timeline = gi->device.createSemaphoreUnique(semInfo, gi->allocator);

    CommandQueue::SyncPoint signalPt {timeline.get(), 1};
    auto                    c1 = q->begin("timeline-signal2");
    q->submit2({c1, {}, {}, {}, {}, {1, &signalPt}});

    CommandQueue::SyncPoint waitPt {timeline.get(), 1};
    auto                    c2 = q->begin("timeline-wait2");
    q->submit2({c2, {}, {}, {1, &waitPt}});

    q->waitIdle();
    CHECK(true); // reaching here means no GPU hang / deadlock
}
