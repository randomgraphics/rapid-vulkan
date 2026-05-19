#include "../rv.h"
#include <memory>
#include <chrono>
#include <iostream>

struct TestVulkanInstance {
    inline static std::unique_ptr<rapid_vulkan::Instance> instance;
    inline static std::unique_ptr<rapid_vulkan::Device>   device;
    inline static bool                                    synchronization2 = false; ///< true if VK_KHR_synchronization2 / VK 1.3 was enabled at device creation
    inline static bool timelineSemaphore = false; ///< true if VK_KHR_timeline_semaphore / VK 1.2 was enabled at device creation
};

struct ScopedTimer {
    std::string                                    name;
    std::chrono::high_resolution_clock::time_point start;

    ScopedTimer(std::string name_): name(name_) { start = std::chrono::high_resolution_clock::now(); }

    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << name << ": " << dur.count() << "ms" << std::endl;
    }
};
