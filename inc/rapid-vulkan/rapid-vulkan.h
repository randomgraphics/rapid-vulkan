/*
MIT License

Copyright (c) 2023 randomgraphics

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef RAPID_VULKAN_H_
#define RAPID_VULKAN_H_

/// A monotonically increasing number that uniquely identify the revision of the header.
#define RAPID_VULKAN_HEADER_REVISION 7

/// \def RAPID_VULKAN_NAMESPACE
/// Define the namespace of rapid-vulkan library.
#ifndef RAPID_VULKAN_NAMESPACE
#define RAPID_VULKAN_NAMESPACE rapid_vulkan
#endif

/// \def RAPID_VULKAN_ENABLE_DEBUG_BUILD
/// Set to non-zero value to enable debug build. Disabled by default.
#ifndef RAPID_VULKAN_ENABLE_DEBUG_BUILD
#define RAPID_VULKAN_ENABLE_DEBUG_BUILD 0
#endif

/// \def RAPID_VULKAN_ENABLE_LOADER
/// Set to 0 to disable built-in Vulkan API loader. Enabled by default.
#ifndef RAPID_VULKAN_ENABLE_LOADER
#define RAPID_VULKAN_ENABLE_LOADER 1
#endif

/// \def RAPID_VULKAN_ENABLE_VMA
/// Set to 0 to disable VMA support. Enabled by default.
#ifndef RAPID_VULKAN_ENABLE_VMA
#define RAPID_VULKAN_ENABLE_VMA 1
#endif

/// \def RAPID_VULKAN_THROW
/// The macro to throw runtime exception.
#ifndef RAPID_VULKAN_THROW
#define RAPID_VULKAN_THROW(...) throw std::runtime_error(__VA_ARGS__)
#endif

/// \def RAPID_VULKAN_BACKTRACE
/// Define custom function to retrieve current call stack and store in std::string.
/// This macro is called when rapid-vulkan encounters critical error, to help
/// quickly identify the source of the error. The default implementation does
/// nothing but return empty string.
#ifndef RAPID_VULKAN_BACKTRACE
#define RAPID_VULKAN_BACKTRACE() std::string("You have to define RAPID_VULKAN_BACKTRACE to retrieve current callstack.")
#endif

/// \def RAPID_VULKAN_LOG_ERROR
/// The macro to log error message. The default implementation prints to stderr.
#ifndef RAPID_VULKAN_LOG_ERROR
#define RAPID_VULKAN_LOG_ERROR(...)    \
    do {                               \
        fprintf(stderr, "[ ERROR ] "); \
        fprintf(stderr, __VA_ARGS__);  \
        fprintf(stderr, "\n");         \
    } while (false)
#endif

/// \def RAPID_VULKAN_LOG_WARNING
/// The macro to log warning message. The default implementation prints to stderr.
#ifndef RAPID_VULKAN_LOG_WARNING
#define RAPID_VULKAN_LOG_WARNING(...)  \
    do {                               \
        fprintf(stderr, "[WARNING] "); \
        fprintf(stderr, __VA_ARGS__);  \
        fprintf(stderr, "\n");         \
    } while (false)
#endif

/// \def RAPID_VULKAN_LOG_INFO
/// The macro to log informational message. The default implementation prints to stdout.
#ifndef RAPID_VULKAN_LOG_INFO
#define RAPID_VULKAN_LOG_INFO(...)    \
    do {                              \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n");        \
    } while (false)
#endif

/// \def RAPID_VULKAN_ASSERT
/// The runtime assert macro for debug build only. This macro has no effect when
/// RAPID_VULKAN_ENABLE_DEBUG_BUILD is 0.
#ifndef RAPID_VULKAN_ASSERT
#define RAPID_VULKAN_ASSERT(expression, ...)                                                     \
    if (!(expression)) {                                                                         \
        auto errorMessage__ = RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__);                       \
        RAPID_VULKAN_LOG_ERROR("Condition " #expression " not met. %s", errorMessage__.c_str()); \
        assert(false);                                                                           \
    } else                                                                                       \
        void(0)
#endif

// ---------------------------------------------------------------------------------------------------------------------
// include vulkan.hpp

#if RAPID_VULKAN_ENABLE_LOADER
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

#ifdef VOLK_H_
// volk.h defines VK_NO_PROTOTYPES to avoid symbol conflicting with vulkan.h. But it
// causes vulkan.hpp to automatically switch to dynamic loader. So we have to undefine it,
// before including vulkan.hpp
#undef VK_NO_PROTOTYPES
#endif

#ifdef __ANDROID__
#include "3rd-party/android/vulkan/vulkan.hpp"
#else
#include <vulkan/vulkan.hpp>
#endif

#ifdef VOLK_H_
// Now restore it.
#define VK_NO_PROTOTYPES
#endif

// ---------------------------------------------------------------------------------------------------------------------
// include VMA header if not already included.
#if RAPID_VULKAN_ENABLE_VMA
#ifndef AMD_VULKAN_MEMORY_ALLOCATOR_H
#define RVI_NEED_VMA_IMPL // this is to tell rapid-vulkan.cpp to include VMA implementation.
#ifdef _MSC_VER
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wnullability-completeness"
#endif
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wundef"
#endif
// Enable these defines to help debug VK memory corruption.
// #ifndef VMA_DEBUG_DETECT_CORRUPTION
// #define VMA_DEBUG_DETECT_CORRUPTION 1
// #define VMA_DEBUG_MARGIN            32
// #endif
#ifndef VMA_DEBUG_ERROR_LOG
#define VMA_DEBUG_ERROR_LOG RAPID_VULKAN_LOG_INFO
#endif
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "3rd-party/vma-3.0.1/vk_mem_alloc.h"
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif // AMD_VULKAN_MEMORY_ALLOCATOR_H
#endif // RAPID_VULKAN_ENABLE_VMA

// ---------------------------------------------------------------------------------------------------------------------
// include other standard/system headers

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <atomic>
#include <vector>
#include <map>
#include <string>
#include <unordered_map>
#include <tuple>
#include <optional>
#include <algorithm>
#include <exception>

// ---------------------------------------------------------------------------------------------------------------------
// RVI stands for Rapid Vulkan Implementation. Macros started with this prefix are reserved for internal use.

#define RVI_NO_COPY(T)                 \
    T(const T &)             = delete; \
    T & operator=(const T &) = delete;

#define RVI_NO_MOVE(T)            \
    T(T &&)             = delete; \
    T & operator=(T &&) = delete;

#define RVI_NO_COPY_NO_MOVE(T) RVI_NO_COPY(T) RVI_NO_MOVE(T)

#define RVI_STR(x) RVI_STR_HELPER(x)

#define RVI_STR_HELPER(x) #x

#define RVI_THROW(...)                                                                                \
    do {                                                                                              \
        std::stringstream ss___;                                                                      \
        ss___ << __FILE__ << "(" << __LINE__ << "): " << RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__); \
        RAPID_VULKAN_LOG_ERROR("%s", ss___.str().data());                                             \
        RAPID_VULKAN_THROW(ss___.str());                                                              \
    } while (false)

#if RAPID_VULKAN_ENABLE_DEBUG_BUILD
// assert is enabled only in debug build
#define RVI_ASSERT RAPID_VULKAN_ASSERT
#else
#define RVI_ASSERT(...) ((void) 0)
#endif

#define RVI_REQUIRE(condition, ...)                                                    \
    do {                                                                               \
        if (!(condition)) {                                                            \
            auto errorMessage__ = RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__);         \
            RVI_THROW("Condition " #condition " not met. %s", errorMessage__.c_str()); \
        }                                                                              \
    } while (false)

#define RVI_VK_REQUIRE(condition, ...)                                                     \
    do {                                                                                   \
        if (VK_SUCCESS != (VkResult) (condition)) {                                        \
            auto errorMessage__ = RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__);             \
            RVI_THROW("Vulkan fuction " #condition " failed. %s", errorMessage__.c_str()); \
        }                                                                                  \
    } while (false)

// Check C++ standard
#ifdef _MSC_VER
#if _MSVC_LANG < 201703L
#error "C++17 is required"
#elif _MSVC_LANG < 202002L
#define RVI_CXX_STANDARD 17
#else
#define RVI_CXX_STANDARD 20
#endif
#else
#if __cplusplus < 201703L
#error "c++17 or higher is required"
#elif __cplusplus < 202002L
#define RVI_CXX_STANDARD 17
#else
#define RVI_CXX_STANDARD 20
#endif
#endif

namespace RAPID_VULKAN_NAMESPACE {

using namespace std::string_literals;

// ---------------------------------------------------------------------------------------------------------------------
/// A utility class used to pass commonly used Vulkan global information around.
struct GlobalInfo {
    const vk::AllocationCallbacks * allocator           = nullptr;
    uint32_t                        apiVersion          = 0;
    vk::Instance                    instance            = nullptr;
    vk::PhysicalDevice              physical            = nullptr;
    vk::Device                      device              = nullptr;
    uint32_t                        graphicsQueueFamily = VK_QUEUE_FAMILY_IGNORED;
#if RAPID_VULKAN_ENABLE_VMA
    VmaAllocator vmaAllocator = nullptr;
#endif

    template<typename T, typename... ARGS>
    void safeDestroy(T & handle, ARGS... args) const {
        if (!handle) return;
        if constexpr (std::is_same_v<T, vk::CommandPool>) {
            device.resetCommandPool(handle, vk::CommandPoolResetFlagBits::eReleaseResources);
            device.destroy(handle, allocator);
        } else if constexpr (std::is_same_v<T, vk::CommandBuffer>) {
            device.freeCommandBuffers(args..., {handle});
        } else if constexpr (std::is_same_v<T, vk::DeviceMemory>) {
            device.freeMemory(handle, allocator);
        } else {
            device.destroy(handle, allocator);
        }
        handle = nullptr;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
/// \def format
/// \brief A printf like string formatting function.
#if __clang__
__attribute__((format(printf, 1, 2)))
#endif
inline std::string
format(const char * format, ...) {
    va_list args;
    va_start(args, format);

    // Get the size of the buffer needed to store the formatted string.
    int size = vsnprintf(NULL, 0, format, args);
    if (size == -1) {
        // Error getting the size of the buffer.
        va_end(args);
        return {};
    }

    // Allocate the buffer.
    std::string buffer(size + 1, '\0');

    // Format the string.
    vsnprintf(&buffer[0], size + 1, format, args);

    // Free the argument list.
    va_end(args);

    // Return the formatted string.
    return buffer;
}

// ---------------------------------------------------------------------------------------------------------------------
/// Overload of format() method for empty parameter list.
inline std::string format() { return ""s; }

// ---------------------------------------------------------------------------------------------------------------------
// Clamp a range of [offset, offset + length) into range of [0, capacity)
/// \return The offset change.
template<typename T>
inline T clampRange(T & offset, T & length, T capacity) {
    auto begin = offset; // remember the original starting point.
    if (length > capacity) length = capacity;
    T end  = offset + length;
    offset = std::clamp<T>(offset, 0, capacity);
    end    = std::clamp<T>(end, offset, capacity);
    RVI_ASSERT(end >= offset);
    length = end - offset;
    return offset - begin;
}

// ---------------------------------------------------------------------------------------------------------------------
// Clamp source and destination ranges. So that (srcOffset, srcOffset + length) and (dstOffset, dstOffset + length) are
// in range of [0, srcCapacity) and [0, dstCapacity) respectively.
template<typename T>
inline T clampRange2(T & srcOffset, T & dstOffset, T & length, const T & srcCapacity, const T & dstCapacity) {
    auto begin = srcOffset; // remember the original starting point.
    dstOffset += clampRange(srcOffset, length, srcCapacity);
    srcOffset += clampRange(dstOffset, length, dstCapacity);
    return srcOffset - begin;
}

// ---------------------------------------------------------------------------------------------------------------------
/// Helper function to set Vulkan opaque handle's name (VK_EXT_debug_utils).
template<typename T>
inline void setVkObjectName(vk::Device device, T handle, const char * name) {
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
    if (!VULKAN_HPP_DEFAULT_DISPATCHER.vkSetDebugUtilsObjectNameEXT) return;
#else
    if (!::vkSetDebugUtilsObjectNameEXT) return;
#endif
    if (!device || !handle || !name) return;

    union HandleAlias {
        uint64_t u64 {};
        void *   object;
    };

    HandleAlias alias;
    alias.object = handle;
    auto info    = vk::DebugUtilsObjectNameInfoEXT().setObjectType(handle.objectType).setObjectHandle(alias.u64).setPObjectName(name);
    device.setDebugUtilsObjectNameEXT(info);
}

// ---------------------------------------------------------------------------------------------------------------------
/// Helper function to set Vulkan opaque handle's name (VK_EXT_debug_utils).
template<typename T>
inline void setVkObjectName(vk::Device device, T handle, std::string name) {
    setVkObjectName(device, handle, name.c_str());
}

// ---------------------------------------------------------------------------------------------------------------------
/// Utility function to enumerate vulkan item/feature/extension list.
template<typename T, typename Q>
inline std::vector<T> completeEnumerate(Q query) {
    // It's possible, though very rare, that the number of items
    // could change. For example, installing something
    // could include new layers that the loader would pick up
    // between the initial query for the count and the
    // request for VkLayerProperties. The loader indicates that
    // by returning a VK_INCOMPLETE status and will update the
    // the count parameter.
    // The count parameter will be updated with the number of
    // entries loaded into the data pointer - in case the number
    // of layers went down or is smaller than the size given.
    std::vector<T> result;
    // if constexpr (std::is_same_v<void, std::result_of_t<Q>>) {
    //     uint32_t count;
    //     query(&count, nullptr);
    //     result.resize(count);
    //     query(&count, result.data());
    // } else {
    vk::Result res;
    do {
        uint32_t count;
        res = query(&count, nullptr);
        if (res != vk::Result::eSuccess) return result;
        if (0 == count) return {};
        result.resize(count);
        res = query(&count, result.data());
    } while (res == vk::Result::eIncomplete);
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
//
std::vector<vk::PhysicalDevice> enumeratePhysicalDevices(vk::Instance instance);

// ---------------------------------------------------------------------------------------------------------------------
// This function currently selects the device with the longest extension list.
vk::PhysicalDevice selectTheMostPowerfulPhysicalDevice(vk::ArrayProxy<const vk::PhysicalDevice> phydevs);

// ---------------------------------------------------------------------------------------------------------------------
//
std::vector<vk::ExtensionProperties> enumerateDeviceExtensions(vk::PhysicalDevice dev);

// ---------------------------------------------------------------------------------------------------------------------
/// The root class of most of the other public classes in this library.
class Root {
public:
    struct ConstructParameters {
        std::string name = "<no-name>"s;
    };

    virtual ~Root() = default;

    /// Name of the object. For debug and log only. Can be any value.
    const std::string & name() const { return _name; }

    /// Set name of the object. For debug and log only. Can be any value.
    void setName(std::string newName) {
        if (newName.empty()) newName = "<no-name>"s;
        if (_name == newName) return;
        std::swap(_name, newName);
        onNameChanged(newName); // newName is currently holding the old name.
    }

    /// @brief Mark the object as "not being automatically deleted when reference count reaches zero".
    /// This is a ver hacky way to keep the object allocated on stack alive even after all references are gone.
    /// The intended scenario is passing a stack allocated object to a function that takes a Ref<T> parameter.
    /// By calling this method, the object will not be deleted even when all references are gone. Instead, it will be
    /// destroyed automatically along with the stack frame, regardless if there are still references to it.
    /// So it is the caller's responsibility to make sure that all references are gone before the stack frame is
    /// destroyed.
    void markAsNotDeleteable() { _noDeleteOnZeroRef = true; }

protected:
    Root(const ConstructParameters & params): _name("<no-name>"s) { setName(params.name); }

    virtual void onNameChanged(const std::string & oldName) { (void) oldName; }

private:
    friend class RefBase;
    std::string           _name;
    std::atomic<uint64_t> _ref               = 0;
    bool                  _noDeleteOnZeroRef = false;
};

// ---------------------------------------------------------------------------------------------------------------------
/// Base class of Ref class.
class RefBase {
protected:
    RefBase() {}

    ~RefBase() {}

    static void addRef(Root * p) {
        RVI_ASSERT(p);
        ++p->_ref;
    }

    static void release(Root * p) {
        RVI_ASSERT(p);
        if (1 == p->_ref.fetch_sub(1)) {
            RVI_ASSERT(0 == p->_ref);
            if (!p->_noDeleteOnZeroRef) delete p;
        }
    }
};

// ---------------------------------------------------------------------------------------------------------------------
/// Reference counter of any class inherited from Root. This class is more efficient on both memory and performance,
/// than std::shared_ptr
template<typename T>
class Ref : public RefBase {
public:
    constexpr Ref() = default;

    Ref(T & t) {
        _ptr = &t;
        addRef(_ptr);
    }

    Ref(T * t) {
        if (!t) return;
        _ptr = t;
        addRef(_ptr);
    }

    ~Ref() { clear(); }

    /// copy constructor
    Ref(const Ref & rhs) {
        if (rhs._ptr) addRef(rhs._ptr);
        _ptr = rhs._ptr;
    }

    /// move constructor
    Ref(Ref && rhs) {
        _ptr     = rhs._ptr;
        rhs._ptr = nullptr;
    }

    void clear() {
        if (_ptr) release(_ptr), _ptr = nullptr;
    }

    bool empty() const { return !_ptr; }

    bool valid() const { return _ptr != nullptr; }

    /// @brief Reset the pointer to a new value.
    /// Passing nullptr is same as calling clear().
    void reset(T * t = nullptr) {
        if (_ptr == t) return;
        clear();
        _ptr = t;
        if (t) addRef(_ptr);
    }

    template<typename T2 = T>
    T2 * get() const {
        return (T2 *) _ptr;
    }

    /// get address of the underlying pointer
    T * const * addr() const {
        RVI_ASSERT(_ptr);
        return &_ptr;
    }

    /// copy operator
    Ref & operator=(const Ref & rhs) {
        if (_ptr == rhs._ptr) return *this;
        if (_ptr) release(_ptr);
        if (rhs._ptr) addRef(rhs._ptr);
        _ptr = rhs._ptr;
        return *this;
    }

    /// move operator
    Ref & operator=(Ref && rhs) {
        if (this != &rhs) {
            if (_ptr) release(_ptr);
            _ptr     = rhs._ptr;
            rhs._ptr = nullptr;
        }
        return *this;
    }

    /// comparison operator
    bool operator==(const Ref & rhs) const { return _ptr == rhs._ptr; }

    /// @brief null equality operator
    /// @return true if null, false otherwise.
    bool operator==(std::nullptr_t) { return _ptr == nullptr; }

    /// comparison operator
    bool operator!=(const Ref & rhs) const { return _ptr != rhs._ptr; }

    /// @brief not null operator.
    /// @return true if not null, false otherwise.
    bool operator!=(std::nullptr_t) { return _ptr != nullptr; }

    /// comparison operator
    bool operator<(const Ref & rhs) const { return _ptr < rhs._ptr; }

    /// @brief boolean cast.
    /// @return true if not null, false otherwise.
    explicit operator bool() const { return 0 != _ptr; }

    /// @brief not operator
    /// @return true if null, false otherwise.
    bool operator!() const { return 0 == _ptr; }

    /// dereference operator
    T * operator->() const {
        RVI_ASSERT(_ptr);
        return _ptr;
    }

    /// dereference operator
    T & operator*() const {
        RVI_ASSERT(_ptr);
        return *_ptr;
    }

private:
    /// @brief Pointer smart pointer is wrapping.
    T * _ptr = nullptr;
};

// // ---------------------------------------------------------------------------------------------------------------------
// /// A wrapper class for VkCommandBuffer
// class CommandBuffer : public Root {
// public:
//     vk::CommandBuffer handle() const { return _handle; }

// protected:
//     CommandBuffer(const Root::ConstructParameters & cp): Root(cp) {}
//     ~CommandBuffer() override = default;

//     vk::CommandBuffer      _handle {};
//     vk::CommandBufferLevel _level = vk::CommandBufferLevel::ePrimary;
// };

class Device;

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkQueue
class CommandQueue : public Root {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        const GlobalInfo * gi     = nullptr;
        uint32_t           family = 0; ///< queue family index
        uint32_t           index  = 0; ///< queue index within family
    };

    struct Desc {
        const GlobalInfo * gi     = nullptr;
        vk::Queue          handle = {};
        uint32_t           family = 0; ///< queue family index
        uint32_t           index  = 0; ///< queue index within family
    };

    struct SubmitParameters {
        /// @brief The command buffers to submit. The command buffer must be allocated out of this queue class.
        /// @todo Submit array of command buffers together.
        const vk::CommandBuffer commands {};

        /// The (optional) fence object to signal once the command buffers have completed execution.
        vk::Fence signalFence = {};

        /// @brief List of semaphores to wait for before executing the command buffers.
        vk::ArrayProxy<const vk::Semaphore> waitSemaphores {};

        /// @brief List of semaphores to signal once the command buffers have completed execution.
        vk::ArrayProxy<const vk::Semaphore> signalSemaphores {};
    };

    CommandQueue(const ConstructParameters &);
    ~CommandQueue() override;

    auto desc() const -> const Desc &;

    /// @brief Begin recording a command buffer.
    /// @todo should return a custom CommandBuffer class.
    auto begin(const char * purpose, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) -> vk::CommandBuffer;

    /// @brief End recording a command buffer. Submit it to the queue for asynchronous processing.
    void submit(const SubmitParameters &);

    /// @brief Wait for the queue to finish processing submitted commands.
    /// @param cb The command buffer to wait for. It must be allocated out of this queue via the begin() call.
    ///           Passing in empty command buffer handle is allowed though. In this case, the function will wait for all
    ///           submitted command buffers to finish.
    void wait(vk::CommandBuffer cb = {});

    auto gi() const -> const GlobalInfo * { return desc().gi; }
    auto family() const -> uint32_t { return desc().family; }
    auto index() const -> uint32_t { return desc().index; }
    auto handle() const -> vk::Queue { return desc().handle; }

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A helper function to insert resource/memory barriers to command buffer
struct Barrier {
    vk::PipelineStageFlags               srcStage     = vk::PipelineStageFlagBits::eAllCommands;
    vk::PipelineStageFlags               dstStage     = vk::PipelineStageFlagBits::eAllCommands;
    vk::DependencyFlags                  dependencies = vk::DependencyFlagBits::eByRegion;
    std::vector<vk::MemoryBarrier>       memories;
    std::vector<vk::BufferMemoryBarrier> buffers;
    std::vector<vk::ImageMemoryBarrier>  images;

    Barrier & clear() {
        srcStage = dstStage = vk::PipelineStageFlagBits::eAllCommands;
        memories.clear();
        buffers.clear();
        images.clear();
        return *this;
    }

    /// @brief Generic function call chain
    /// This is to make it possible to call any function in a call chain: Barrier{}.m(...).b(...).i(...).p([](auto & b) {...});
    template<typename PROC>
    Barrier & p(PROC proc) {
        proc(*this);
        return *this;
    }

    /// @brief Setup a full pipeline barrier that blocks all stages.
    Barrier & full() {
        clear();
        auto flags = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        m(flags, flags);
        return *this;
    }

    /// @brief Add a memory barrier
    Barrier & m(vk::AccessFlags srcAccess, vk::AccessFlags dstAccess) {
        memories.emplace_back(vk::MemoryBarrier(srcAccess, dstAccess));
        return *this;
    }

    /// @brief Add a buffer barrier
    Barrier & b(const vk::BufferMemoryBarrier & bmb) {
        if (!bmb.buffer) return *this;
        buffers.emplace_back(bmb);
        return *this;
    }

    /// @brief Add a buffer barrier
    Barrier & b(vk::Buffer buffer, vk::AccessFlags srcAccess = vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead,
                vk::AccessFlags dstAccess = vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead, vk::DeviceSize offset = 0,
                vk::DeviceSize size = VK_WHOLE_SIZE) {
        return b(vk::BufferMemoryBarrier {srcAccess, dstAccess, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, buffer, offset, size});
    }

    /// @brief Add an image barrier
    Barrier & i(vk::Image image, vk::AccessFlags srcAccess, vk::AccessFlags dstAccess, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                vk::ImageSubresourceRange subresourceRange) {
        if (!image) return *this;
        images.emplace_back(
            vk::ImageMemoryBarrier {srcAccess, dstAccess, oldLayout, newLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, subresourceRange});
        return *this;
    }

    /// @brief Add an image barrier
    Barrier & i(vk::Image image, vk::AccessFlags srcAccess, vk::AccessFlags dstAccess, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                vk::ImageAspectFlags aspect) {
        vk::ImageSubresourceRange range = {aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
        return i(image, srcAccess, dstAccess, oldLayout, newLayout, range);
    }

    /// @brief Set pipeline stages
    Barrier & s(vk::PipelineStageFlags src, vk::PipelineStageFlags dst) {
        srcStage = src;
        dstStage = dst;
        return *this;
    }

    /// @brief Write barriers to command buffer
    void cmdWrite(vk::CommandBuffer cb) const {
        if (memories.empty() && buffers.empty() && images.empty()) return;
        cb.pipelineBarrier(srcStage, dstStage, dependencies, memories, buffers, images);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkBuffer
class Buffer : public Root {
public:
    struct Desc {
        vk::Buffer              handle = {};
        vk::DeviceSize          size   = 0;
        vk::BufferUsageFlags    usage  = {};
        vk::MemoryPropertyFlags memory = {};

        /// @brief Check if the buffer is coherently mappable.
        bool mappable() const { return (memory & vk::MemoryPropertyFlagBits::eHostVisible) && (memory & vk::MemoryPropertyFlagBits::eHostCoherent); }
    };

    /// @brief Parameters for creating a new buffer
    struct ConstructParameters : public Root::ConstructParameters {
        const GlobalInfo *      gi     = nullptr;
        vk::DeviceSize          size   = 0; ///< size of the buffer in bytes.
        vk::BufferUsageFlags    usage  = {};
        vk::MemoryPropertyFlags memory = vk::MemoryPropertyFlagBits::eDeviceLocal;
        vk::MemoryAllocateFlags alloc  = {};

        ConstructParameters & setSize(vk::DeviceSize v) {
            size = v;
            return *this;
        }

        ConstructParameters & setUsage(vk::BufferUsageFlags v) {
            usage = v;
            return *this;
        }

        ConstructParameters & setStaging() {
            usage  = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
            memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            return *this;
        }

        ConstructParameters & setStorage() {
            usage = vk::BufferUsageFlagBits::eStorageBuffer;
            return *this;
        }

        ConstructParameters & setUniform() {
            usage = vk::BufferUsageFlagBits::eUniformBuffer;
            return *this;
        }

        ConstructParameters & setVertex() {
            usage = vk::BufferUsageFlagBits::eVertexBuffer;
            return *this;
        }

        ConstructParameters & setIndex() {
            usage = vk::BufferUsageFlagBits::eIndexBuffer;
            return *this;
        }
    };

    /// @brief Parameters for importing an existing buffer handle.
    struct ImportParameters : public Root::ConstructParameters {
        const GlobalInfo * gi;
        Desc               desc;
    };

    /// @brief Parameters to copy data from one buffer to another.
    struct CopyParameters {
        vk::CommandBuffer cb          = {};                 ///< command buffer to record the copy command.
        vk::Buffer        dst         = {};                 ///< the destination buffer
        vk::DeviceSize    dstCapacity = 0;                  ///< size of the destination buffer, in bytes. Must be positive number.
        vk::DeviceSize    dstOffset   = 0;                  ///< offset to the beginning of the destination buffer, in bytes.
        vk::DeviceSize    srcOffset   = 0;                  ///< offset to the beginning of the source buffer, in bytes.
        vk::DeviceSize    size        = vk::DeviceSize(-1); ///< size of the data to be copied, in bytes.
    };

    /// @brief Parameters to synchronously update buffer content.
    struct SetContentParameters {
        uint32_t       queueFamily = 0;
        uint32_t       queueIndex  = 0;
        const void *   data        = {};
        vk::DeviceSize size        = 0; ///< size of the data to be written, in bytes.
        vk::DeviceSize offset      = 0; ///< byte offset of the destination buffer where the data will be written to.

        SetContentParameters & setQueue(CommandQueue & q) {
            queueFamily = q.family();
            queueIndex  = q.index();
            return *this;
        }

        SetContentParameters & setData(const void * data_, vk::DeviceSize size_) {
            data = data_;
            size = size_;
            return *this;
        }

        template<typename T>
        SetContentParameters & setData(vk::ArrayProxy<T> data_) {
            data = data_.data();
            size = data_.size() * sizeof(T);
            return *this;
        }

        SetContentParameters & setOffset(vk::DeviceSize o) {
            offset = o;
            return *this;
        }
    };

    /// @brief Parameters to synchonously read data from buffer.
    struct ReadParameters {
        uint32_t       queueFamily = 0;
        uint32_t       queueIndex  = 0;
        vk::DeviceSize offset      = 0;                  ///< byte offset of the source buffer where the data will be read from.
        vk::DeviceSize size        = vk::DeviceSize(-1); ///< size of the data to be read, in bytes.

        ReadParameters & setQueue(CommandQueue & q) {
            queueFamily = q.family();
            queueIndex  = q.index();
            return *this;
        }

        ReadParameters & setRange(vk::DeviceSize o, vk::DeviceSize s = vk::DeviceSize(-1)) {
            offset = o;
            size   = s;
            return *this;
        }
    };

    struct MapParameters {
        vk::DeviceSize offset = 0;                  ///< the offset of the mapped area, in bytes.
        vk::DeviceSize size   = vk::DeviceSize(-1); ///< the size of the mapped area, in bytes.
    };

    struct MappedResult {
        uint8_t *      data   = nullptr; ///< the mapped data buffer. If null, then the mapping failed.
        vk::DeviceSize offset = 0;       ///< the offset of the mapped area, in bytes.
        vk::DeviceSize size   = 0;       ///< the size of the mapped area, in bytes.
    };

    /// @brief A helper class that maps the buffer and and automatically unmap the buffer when destroyed.
    template<typename T = uint8_t>
    struct Map {
        Buffer *       buffer = nullptr;
        T *            data   = nullptr;
        vk::DeviceSize offset = 0; ///< offset of the mapped area, in unit of T.
        vk::DeviceSize length = 0; ///< size of the mapped area, in unit of T.

        RVI_NO_COPY(Map);
        RVI_NO_MOVE(Map);

        Map(Buffer & buffer_, vk::DeviceSize offset = 0, vk::DeviceSize size = vk::DeviceSize(-1)) {
            RVI_ASSERT(empty());
            auto mapped = buffer_.map({offset, size});
            if (!mapped.data) return;
            buffer = &buffer_;
            data   = (T *) mapped.data;
            offset = mapped.offset / sizeof(T);
            length = mapped.size / sizeof(T);
        }

        ~Map() { unmap(); }

        bool empty() const { return !buffer || !data || !length; }

        void unmap() {
            if (buffer) {
                buffer->unmap();
                // // if (owner->allocation) {
                // //     PH_REQUIRE(owner->global->vmaAllocator);
                // //     vmaUnmapMemory(owner->global->vmaAllocator, owner->allocation);
                // // } else {
                // vkUnmapMemory(owner->global->device, owner->memory);
                // // }
                // owner->mapped = false;
                buffer = nullptr;
            }
            RVI_ASSERT(empty());
        }

        operator bool() const { return !empty(); }
    };

    Buffer(const ConstructParameters &);

    Buffer(const ImportParameters &);

    ~Buffer() override;

    auto desc() const -> const Desc &;
    void cmdCopy(const CopyParameters &);
    void setContent(const SetContentParameters &);
    auto readContent(const ReadParameters &) -> std::vector<uint8_t>;
    auto map(const MapParameters &) -> MappedResult;
    void unmap();

    vk::Buffer handle() const { return desc().handle; }

    operator vk::Buffer() const { return desc().handle; }

    operator VkBuffer() const { return desc().handle; }

protected:
    void onNameChanged(const std::string &) override;

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkImage and VkImageView
class Image : public Root {
public:
    struct ConstructParameters : Root::ConstructParameters {
        const GlobalInfo *      gi     = nullptr;
        vk::ImageCreateInfo     info   = vk::ImageCreateInfo({}, vk::ImageType::e2D, vk::Format::eR8G8B8A8Unorm, {1, 1, 1}, 1, 1, vk::SampleCountFlagBits::e1,
                                                             vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled);
        vk::MemoryPropertyFlags memory = vk::MemoryPropertyFlagBits::eDeviceLocal;
        vk::MemoryAllocateFlags alloc  = {};

        ConstructParameters & set2D(size_t w, size_t h, size_t a = 1) {
            info.imageType     = vk::ImageType::e2D;
            info.extent.width  = (uint32_t) w;
            info.extent.height = (uint32_t) h;
            info.extent.depth  = 1;
            info.arrayLayers   = (uint32_t) a;
            info.flags &= ~vk::ImageCreateFlagBits::eCubeCompatible;
            return *this;
        }

        ConstructParameters & setDepth(size_t w, size_t h, vk::Format f = vk::Format::eD24UnormS8Uint) {
            set2D(w, h).setFormat(f);
            info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
            return *this;
        }

        ConstructParameters & setCube(size_t w) {
            info.imageType     = vk::ImageType::e2D;
            info.extent.width  = (uint32_t) w;
            info.extent.height = (uint32_t) w;
            info.extent.depth  = 1;
            info.arrayLayers   = 6;
            info.flags |= vk::ImageCreateFlagBits::eCubeCompatible;
            return *this;
        }

        ConstructParameters & setFormat(vk::Format f) {
            info.format = f;
            return *this;
        }

        ConstructParameters & setLevels(size_t l) {
            info.mipLevels = (uint32_t) l;
            return *this;
        }

        ConstructParameters & setLayers(size_t f) {
            info.arrayLayers = (uint32_t) f;
            return *this;
        }

        ConstructParameters & setLinear() {
            info.tiling = vk::ImageTiling::eLinear;
            return *this;
        }

        ConstructParameters & setUsage(vk::ImageUsageFlags flags) {
            info.usage = flags;
            return *this;
        }

        ConstructParameters & addUsage(vk::ImageUsageFlags flags) {
            info.usage |= flags;
            return *this;
        }

        ConstructParameters & clearUsage() {
            info.usage = (vk::ImageUsageFlagBits) 0;
            return *this;
        }

        ConstructParameters & renderTarget() {
            info.usage |= vk::ImageUsageFlagBits::eColorAttachment;
            return *this;
        }

        ConstructParameters & depth() {
            info.usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
            return *this;
        }

        ConstructParameters & input() {
            info.usage |= vk::ImageUsageFlagBits::eInputAttachment;
            return *this;
        }

        ConstructParameters & texture() {
            info.usage |= vk::ImageUsageFlagBits::eSampled;
            return *this;
        }

        ConstructParameters & storage() {
            info.usage |= vk::ImageUsageFlagBits::eStorage;
            return *this;
        }

        ConstructParameters & setInitialLayout(vk::ImageLayout l) {
            info.initialLayout = l;
            return *this;
        }

        ConstructParameters & setMemoryFlags(vk::MemoryPropertyFlags property, vk::MemoryAllocateFlags alloc_) {
            memory = property;
            alloc  = alloc_;
            return *this;
        }

        bool isCube() const {
            return vk::ImageType::e2D == info.imageType && info.extent.width == info.extent.height && 1 == info.extent.depth && 6 == info.arrayLayers &&
                   (vk::ImageCreateFlagBits::eCubeCompatible & info.flags);
        }

        bool isCubeOrCubeArray() const {
            return vk::ImageType::e2D == info.imageType && info.extent.width == info.extent.height && 1 == info.extent.depth && 6 <= info.arrayLayers &&
                   0 == (info.arrayLayers % 6) && (vk::ImageCreateFlagBits::eCubeCompatible & info.flags);
        }
    };

    struct Desc {
        vk::Image               handle      = {};
        vk::ImageType           type        = vk::ImageType::e2D;
        vk::Format              format      = vk::Format::eR8G8B8A8Unorm;
        vk::Extent3D            extent      = {1, 1, 1};
        uint32_t                mipLevels   = 1;
        uint32_t                arrayLayers = 1;
        vk::SampleCountFlagBits samples     = vk::SampleCountFlagBits::e1;
    };

    struct GetViewParameters {
        vk::ImageViewType         type   = (vk::ImageViewType) -1; ///< set to -1 to use the default view type.
        vk::Format                format = vk::Format::eUndefined; ///< set to eUndefined to use image's format.
        vk::ImageSubresourceRange range  = {{}, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};

        GetViewParameters & setType(vk::ImageViewType t) {
            type = t;
            return *this;
        }

        GetViewParameters & setFormat(vk::Format f) {
            format = f;
            return *this;
        }

        GetViewParameters & setRange(const vk::ImageSubresourceRange & r) {
            range = r;
            return *this;
        }
    };

    struct Rect3D {
        uint32_t x = 0;            ///< x offset of the area.
        uint32_t y = 0;            ///< y offset of the area.
        uint32_t z = 0;            ///< z offset of the area.
        uint32_t w = uint32_t(-1); ///< width of the area.
        uint32_t h = uint32_t(-1); ///< height of the area
        uint32_t d = uint32_t(-1); ///< depth of the area
    };

    struct SetContentParameters {
        uint32_t     queueFamily = 0;
        uint32_t     queueIndex  = 0;
        uint32_t     mipLevel    = 0;
        uint32_t     arrayLayer  = 0;
        Rect3D       area        = {};
        size_t       pitch       = 0; ///< size in byte of a pixel block rows. This is multiple scan line of pixels for compressed texture.
        const void * pixels      = nullptr;

        SetContentParameters & setQueue(CommandQueue & q) {
            queueFamily = q.family();
            queueIndex  = q.index();
            return *this;
        }

        SetContentParameters & setPixels(const void * p) {
            pixels = p;
            return *this;
        }
    };

    struct ReadContentParameters {
        uint32_t queueFamily = 0;
        uint32_t queueIndex  = 0;

        ReadContentParameters & setQueue(CommandQueue & q) {
            queueFamily = q.family();
            queueIndex  = q.index();
            return *this;
        }
    };

    struct SubresourceContent {
        uint32_t       mipLevel;
        uint32_t       arrayLayer;
        vk::Extent3D   extent; ///< size of the image in pixels.
        uint32_t       pitch;  ///< size in byte of one pixel block.
        vk::DeviceSize offset; ///< offset in bytes into the pixel storage buffer.
    };

    struct Content {
        /// @brief The format of the pixel.
        vk::Format format;

        /// @brief The storage of all pixels.
        std::vector<uint8_t> storage;

        /// @brief content of each subresource. index by (mipLevel * layerCount + arrayLayer)
        std::vector<SubresourceContent> subresources;
    };

    /// @brief A utility function to determine image aspect flags from a format.
    /// @param format The pixel format of the image.
    /// @param hint   The hint of the aspect flags. The function will try to use this hinted aspect flag, as long as it is compatible the format.
    ///               Set to vk::ImageAspectFlagBits::eNone to let the function determine the aspect flags.
    static vk::ImageAspectFlags determineImageAspect(vk::Format format, vk::ImageAspectFlags hint = vk::ImageAspectFlagBits::eNone);

    Image(const ConstructParameters &);

    ~Image();

    /// @brief Get basic properties of the image.
    const Desc & desc() const;

    /// @brief Retrieve a vk::ImageView of the image.
    vk::ImageView getView(const GetViewParameters &) const;

    /// @brief Synchronously set content of one subresource
    /// This method, if succeeded, will transfer the subresource into vk::ImageLayout::eTransferDstOptimal layout.
    void setContent(const SetContentParameters &);

    /// @brief Synchronously read content of the whole image.
    /// This method, if succeeded, will transfer the image into vk::ImageLayout::eTransferSrcOptimal layout.
    Content readContent(const ReadContentParameters &);

    vk::Image handle() const { return desc().handle; }

    operator vk::Image() const { return desc().handle; }

    operator VkImage() const { return desc().handle; }

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkSampler
class Sampler : public Root {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        const GlobalInfo *    gi = {};
        vk::SamplerCreateInfo info;

        ConstructParameters & setLinear() {
            info.magFilter = info.minFilter = vk::Filter::eLinear;
            info.mipmapMode                 = vk::SamplerMipmapMode::eLinear;
            return *this;
        }

        ConstructParameters & setNearest() {
            info.magFilter = info.minFilter = vk::Filter::eNearest;
            info.mipmapMode                 = vk::SamplerMipmapMode::eNearest;
            return *this;
        }
    };

    Sampler(const ConstructParameters & cp): Root(cp), _gi(cp.gi) {
        _handle = _gi->device.createSampler(cp.info, _gi->allocator);
        onNameChanged("");
    }

    ~Sampler() override { _gi->safeDestroy(_handle); }

    vk::Sampler handle() const { return _handle; }

    operator vk::Sampler() const { return _handle; }

    operator VkSampler() const { return _handle; }

protected:
    void onNameChanged(const std::string &) override {
        if (_handle) setVkObjectName(_gi->device, _handle, name());
    }

private:
    const GlobalInfo * _gi = nullptr;
    vk::Sampler        _handle;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkShaderModule
class Shader : public Root {
public:
    static Shader EMPTY;

    struct ConstructParameters : public Root::ConstructParameters {
        const GlobalInfo *             gi = nullptr;
        vk::ArrayProxy<const uint32_t> spirv {};
        const char *                   entry = "main";

        ConstructParameters & setGi(const GlobalInfo * v) {
            gi = v;
            return *this;
        }

        template<typename T, size_t C>
        ConstructParameters & setSpirv(const T (&data)[C]) {
            spirv = vk::ArrayProxy<const uint32_t>(C * sizeof(T) / sizeof(uint32_t), (const uint32_t *) data);
            return *this;
        }

        template<typename T>
        ConstructParameters & setSpirv(size_t countInUnitOfT, const T * data) {
            spirv = vk::ArrayProxy<const uint32_t>(countInUnitOfT * sizeof(T) / sizeof(uint32_t), (const uint32_t *) data);
            return *this;
        }

        // template<typename T>
        // ConstructParameters & setSpirv(vk::ArrayProxy<const T> blob) {
        //     spirv = vk::ArrayProxy<const uint32_t>(blob.size() * sizeof(T) / sizeof(uint32_t), (const uint32_t*)blob.data());
        //     return *this;
        // }
    };

    Shader(const ConstructParameters &);

    ~Shader() override;

    const GlobalInfo * gi() const { return _gi; }

    vk::ShaderModule handle() const { return _handle; }

    const std::string & entry() const { return _entry; }

    vk::ArrayProxy<const uint32_t> spirv() const { return _spirv; }

private:
    const GlobalInfo *    _gi = nullptr;
    vk::ShaderModule      _handle {};
    std::string           _entry;
    std::vector<uint32_t> _spirv;
};

// ---------------------------------------------------------------------------------------------------------------------
/// VkFramebuffer wrapper class
class RenderPass : public Root {
public:
    struct SubpassParameters {
        std::vector<vk::AttachmentReference>   colors;
        std::optional<vk::AttachmentReference> depth;
        std::vector<vk::AttachmentReference>   inputs;
        vk::SubpassDescriptionFlags            flags = {};
    };

    struct ConstructParameters : public Root::ConstructParameters {
        const GlobalInfo *                     gi    = nullptr;
        vk::RenderPassCreateFlags              flags = {};
        std::vector<vk::AttachmentDescription> attachments {};
        std::vector<SubpassParameters>         subpasses {};
        std::vector<vk::SubpassDependency>     dependencies {};

        /// @brief Setup a simple single pass render pass.
        ConstructParameters & simple(vk::ArrayProxy<const vk::Format> colors, vk::Format depth = vk::Format::eUndefined, bool clear = true, bool store = true);
    };

    RenderPass(const ConstructParameters &);

    ~RenderPass();

    void cmdBegin(vk::CommandBuffer, vk::RenderPassBeginInfo) const;

    void cmdNext(vk::CommandBuffer) const;

    void cmdEnd(vk::CommandBuffer) const;

    vk::RenderPass handle() const { return _handle; }

    operator vk::RenderPass() const { return _handle; }

    operator VkRenderPass() const { return _handle; }

protected:
    void onNameChanged(const std::string &) override;

private:
    const GlobalInfo * _gi     = nullptr;
    vk::RenderPass     _handle = {};
#if RAPID_VULKAN_ENABLE_DEBUG_BUILD
    ConstructParameters _cp; // keep construct parameters around for debug purpose only.
#endif
};

// ---------------------------------------------------------------------------------------------------------------------
/// VkFramebuffer wrapper class
class Framebuffer : public Root {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        const GlobalInfo *         gi   = nullptr;
        vk::RenderPass             pass = {};
        std::vector<vk::ImageView> attachments {};
        size_t                     width  = 1;
        size_t                     height = 1;
        size_t                     layers = 1;

        ConstructParameters & setRenderPass(vk::RenderPass v) {
            pass = v;
            return *this;
        }

        ConstructParameters & addImage(Image & image);

        ConstructParameters & addImageView(vk::ImageView view) {
            attachments.emplace_back(view);
            return *this;
        }

        ConstructParameters & setExtent(size_t w, size_t h, size_t l = 1) {
            width  = w;
            height = h;
            layers = l;
            return *this;
        }
    };

    Framebuffer(const ConstructParameters &);

    ~Framebuffer();

    vk::Framebuffer handle() const { return _handle; }

    operator vk::Framebuffer() const { return _handle; }

    operator VkFramebuffer() const { return _handle; }

protected:
    void onNameChanged(const std::string &) override;

private:
    const GlobalInfo * _gi     = nullptr;
    vk::Framebuffer    _handle = {};
};

// ---------------------------------------------------------------------------------------------------------------------
/// A utility class that represents the full layout of a pipeline object.
struct PipelineReflection {
    typedef vk::DescriptorSetLayoutBinding Descriptor;

    /// Collection of descriptors in one set. We can't use binding point as key, since multiple shader variable might bind to same set and binding point.
    typedef std::unordered_map<std::string, Descriptor> DescriptorSet;

    /// Collection of descriptor sets indexed by their set index in shader.
    typedef std::vector<DescriptorSet> DescriptorLayout;

    typedef vk::PushConstantRange Constant;

    /// Collection of push constants.
    typedef std::unordered_map<std::string, Constant> ConstantLayout;

    /// Properties of vertex shader input.
    struct VertexShaderInput {
        uint32_t   location = 0;
        vk::Format format   = vk::Format::eUndefined;
    };

    /// Collection of vertex shader input.
    typedef std::unordered_map<std::string, VertexShaderInput> VertexLayout;

    std::string      name; ///< name of the program that this reflect is from. this field is for logging and debugging.
    DescriptorLayout descriptors;
    ConstantLayout   constants;
    VertexLayout     vertex;
};

// ---------------------------------------------------------------------------------------------------------------------
/// @brief View to a sub-range of a buffer.
struct BufferView {
    vk::Buffer     buffer = {};
    vk::DeviceSize offset = 0;
    vk::DeviceSize size   = vk::DeviceSize(-1);

    bool operator==(const BufferView & rhs) const {
        if (this == &rhs) return true;
        if (buffer != rhs.buffer) return false;
        if (offset != rhs.offset) return false;
        return size == rhs.size;
    }

    bool operator!=(const BufferView & rhs) const { return !(*this == rhs); }
};

// ---------------------------------------------------------------------------------------------------------------------
/// @brief A structure to represent one of the following 3 items: image, sampler or combined-image-sampler
///
/// The struct represents an image (either sampled image or storage image), when sampler is empty and image is not.
///
/// The struct represents an sampler, when image is empty and sampler is not.
///
/// The struct represents an combined-image-sampler, when both image and sampler are valid.
///
/// The case that both image and sampler are empty is not allowed and could trigger undefined behavior.
struct ImageSampler {
    /// @brief The view of the image
    vk::ImageView imageView {};

    /// @brief The layout of the image view. Ignored when image view is empty.
    vk::ImageLayout imageLayout = vk::ImageLayout::eUndefined;

    /// @brief The sampler object.
    vk::Sampler sampler {};

    bool operator==(const ImageSampler & rhs) const {
        if (this == &rhs) return true;
        if (imageView != rhs.imageView) return false;
        return sampler != rhs.sampler;
    }

    bool operator!=(const ImageSampler & rhs) const { return !(*this == rhs); }
};

// ---------------------------------------------------------------------------------------------------------------------
/// Represent a single pipeline argument
class Argument {
public:
    RVI_NO_COPY_NO_MOVE(Argument);

    /// @brief Set value of buffer argument. No effect, if the argument is not a buffer.
    void b(vk::ArrayProxy<const BufferView>);

    /// @brief Set value of image/sampler argument. No effect, if the argument is not a image/sampler
    void i(vk::ArrayProxy<const ImageSampler>);

    /// @brief Set value of push constant. No effect, if the argument is not a push constant.
    void c(size_t offset, size_t size, const void * data);

protected:
    Argument();
    ~Argument(); // No need make this virtual, since we'll always delete it through the derived class.

    class Impl;
    Impl * _impl = nullptr;

    friend class PipelineLayout;
};

// ---------------------------------------------------------------------------------------------------------------------
/// Represent a full set of arguments that can be applied to a pipeline
class ArgumentPack : public Root {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        // reserved for future use.
    };

    ArgumentPack(const ConstructParameters &);

    ~ArgumentPack();

    /// @brief clear all arguments.
    void clear();

    /// @brief Set value of buffer argument. If the argument has not been set before, a new argument will be created.
    void b(const std::string & name, vk::ArrayProxy<const BufferView>);

    /// @brief Set value of image/sampler argument. If the argument has not been set before, a new argument will be created.
    void i(const std::string & name, vk::ArrayProxy<const ImageSampler>);

    /// @brief Set value of push constant. If the argument has not been set before, a new argument will be created.
    void c(const std::string & name, size_t offset, size_t size, const void * data);

    /// @brief Set value of push constant. If the argument has not been set before, a new argument will be created.
    template<typename T>
    void c(const std::string & name, size_t offset, vk::ArrayProxy<T> data) {
        return c(name, offset, data.size() * sizeof(T), data.data());
    }

    /// @brief Get argument by name.
    /// The returned argument instance can be used to set value of that argument w/o paying the cost of string hashing.
    /// If the argument has not been set before, a new argument will be created and returned.
    Argument * get(const std::string & name);

    /// @brief Get an existing argument by name. Returns nullptr if the argument has not been set.
    const Argument * get(const std::string & name) const;

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkPipelineLayout
class PipelineLayout : public Root {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        vk::ArrayProxy<const Shader * const> shaders;
    };

    PipelineLayout(const ConstructParameters &);

    ~PipelineLayout() override;

    /// @brief Returns the underlying Vulkan handle.
    vk::PipelineLayout handle() const;

    /// @brief Returns the pipeline reflection object that is used to construct this pipeline layout.
    const PipelineReflection & reflection() const;

    /// @brief Bind argument pack to the command buffer
    /// After this method succeeded (returns true), it is ready to bind issue draw/dispatch commands.
    bool cmdBind(vk::CommandBuffer, vk::PipelineBindPoint, const ArgumentPack &) const;

protected:
    void onNameChanged(const std::string &) override;

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkPipeline
class Pipeline : public Root {
public:
    ~Pipeline() override;

    const PipelineLayout & layout() const;

    void cmdBind(vk::CommandBuffer cb, const ArgumentPack & ap) const;

protected:
    Pipeline(const std::string & name, vk::ArrayProxy<const Shader * const> shaders);

    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// Wrapper of graphics pipeline object
class GraphicsPipeline : public Pipeline {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        vk::RenderPass                                     pass {};
        uint32_t                                           subpass {};
        const Shader *                                     vs {};
        const Shader *                                     fs {};
        std::vector<vk::VertexInputAttributeDescription>   va {};
        std::vector<vk::VertexInputBindingDescription>     vb {};
        vk::PipelineInputAssemblyStateCreateInfo           ia {defaultIAStates()};
        vk::PipelineTessellationStateCreateInfo            tess {};
        std::vector<vk::Viewport>                          viewports {}; ///< viewports
        std::vector<vk::Rect2D>                            scissors {};  ///< scissors
        vk::PipelineRasterizationStateCreateInfo           rast {defaultRastStates()};
        vk::PipelineMultisampleStateCreateInfo             msaa {};
        vk::PipelineDepthStencilStateCreateInfo            depth {};
        std::vector<vk::PipelineColorBlendAttachmentState> attachments {defaultAttachment()};
        std::array<float, 4>                               blendConstants {};
        std::vector<vk::DynamicState>                      dynamic {};
        vk::Pipeline                                       baseHandle {};
        uint32_t                                           baseIndex {};

        ConstructParameters & setRenderPass(vk::RenderPass pass_, size_t sub = 0) {
            pass    = pass_;
            subpass = (uint32_t) sub;
            return *this;
        }

        ConstructParameters & setVS(const Shader * s) {
            vs = s;
            return *this;
        }

        ConstructParameters & setFS(const Shader * s) {
            fs = s;
            return *this;
        }

        ConstructParameters & dynamicTopology() {
            dynamic.push_back(vk::DynamicState::ePrimitiveTopology);
            return *this;
        }

        ConstructParameters & dynamicViewport() {
            dynamic.push_back(vk::DynamicState::eViewportWithCount);
            viewports.clear();
            return *this;
        }

        ConstructParameters & dynamicScissor() {
            dynamic.push_back(vk::DynamicState::eScissorWithCount);
            scissors.clear();
            return *this;
        }

        static constexpr vk::PipelineInputAssemblyStateCreateInfo defaultIAStates() {
            return vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleList);
        }

        static constexpr vk::PipelineRasterizationStateCreateInfo defaultRastStates() {
            vk::PipelineRasterizationStateCreateInfo r;
            r.setLineWidth(1.0f);
            return r;
        }

        static const vk::PipelineColorBlendAttachmentState defaultAttachment() {
            return vk::PipelineColorBlendAttachmentState().setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                                             vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        }
    };

    struct IndexBuffer {
        /// @brief Handle of the index buffer.
        vk::Buffer buffer = VK_NULL_HANDLE;

        /// @brief Byte offset of the first index. Ignored if buffer is empty.
        vk::DeviceSize offset = 0;

        /// @brief Index type;
        vk::IndexType indexType = vk::IndexType::eUint16;

        /// @brief Get size/stride of the index based on the type.
        size_t indexStride() const {
            switch (indexType) {
            case vk::IndexType::eUint16:
                return sizeof(uint16_t);
            case vk::IndexType::eUint32:
                return sizeof(uint32_t);
            case vk::IndexType::eUint8EXT:
                return 1;
            default:
                return 0;
            }
        }
    };

    struct DrawParameters {
        /// Vertex buffer list of the draw.
        vk::ArrayProxy<const BufferView> vertexBuffers;

        /// Index buffer of the draw. If empty, then the draw is considered non-indexed.
        IndexBuffer indexBuffer;

        /// Instance count. Default value is 1.
        uint32_t instanceCount = 1;

        /// Index of the first instance. Default is 0.
        uint32_t firstInstance = 0;

        union {
            /// Index count for indexed draw.
            uint32_t indexCount = 0;

            /// Vertex count for non-indexed draw.
            uint32_t vertexCount;
        };

        union {
            /// Index of the first vertex for non-indexed draw.
            uint32_t firstVertex = 0;

            /// Index of the first index of the indexed draw.
            uint32_t firstIndex;
        };

        /// Vertex offset of indexed draw. Ignored for non-indexed draw.
        int32_t vertexOffset = 0;

        DrawParameters & setNonIndexed(size_t vertexCount_, size_t firstVertex_ = 0) {
            indexBuffer.buffer = VK_NULL_HANDLE;
            vertexCount        = (uint32_t) vertexCount_;
            firstVertex        = (uint32_t) firstVertex_;
            return *this;
        }

        DrawParameters & setIndexed(const IndexBuffer & ib, size_t indexCount_, size_t firstIndex_ = 0, int32_t vertexOffset_ = 0) {
            RAPID_VULKAN_ASSERT(ib.buffer, "Set indexed draw but with an empty index buffer?");
            indexBuffer  = ib;
            indexCount   = (uint32_t) indexCount_;
            firstIndex   = (uint32_t) firstIndex_;
            vertexOffset = vertexOffset_;
            return *this;
        }

        DrawParameters & setInstance(size_t count, size_t first = 0) {
            instanceCount = (uint32_t) count;
            firstInstance = (uint32_t) first;
            return *this;
        }
    };

    GraphicsPipeline(const ConstructParameters &);

    void cmdDraw(vk::CommandBuffer, const DrawParameters &);
};

// ---------------------------------------------------------------------------------------------------------------------
/// Wrapper class of compute pipeline object
class ComputePipeline : public Pipeline {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        /// Pointer to the computer shader. Must be non-null.
        const Shader * cs = nullptr;
    };

    struct DispatchParameters {
        size_t width  = 1;
        size_t height = 1;
        size_t depth  = 1;
    };

    ComputePipeline(const ConstructParameters &);

    void cmdDispatch(vk::CommandBuffer, const DispatchParameters &);
};

// ---------------------------------------------------------------------------------------------------------------------
/// Wrapper class of swapchain object
class Swapchain : public Root {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        const GlobalInfo * gi                  = {};
        vk::SurfaceKHR     surface             = {};
        uint32_t           presentQueueFamily  = VK_QUEUE_FAMILY_IGNORED; ///< Family index of the present queue.
        uint32_t           presentQueueIndex   = 0;                       ///< Index of the present queue.
        uint32_t           graphicsQueueFamily = VK_QUEUE_FAMILY_IGNORED; ///< family index of graphics queue, if different from the present queue.
        uint32_t           graphicsQueueIndex  = 0; ///< Index of the graphics queue. Ignored if graphicsQueueFamily is VK_QUEUE_FAMILY_IGNORED.
        size_t             width               = 0; ///< width of the swapchain. 0 means surface/window width.
        size_t             height              = 0; ///< height of the swapchain. 0 means using surface/window height.
        size_t             maxFramesInFlight   = 1; ///< Number of frames in flight. Must be at least 1.
        bool               vsync               = true;

        /// Specify format of backbuffers. If set to undefined, then the first supported format will be used.
        vk::Format backbufferFormat = vk::Format::eUndefined;

        /// Format of the depth buffer. Set to undefined, if you don't need depth buffer.
        vk::Format depthStencilFormat = vk::Format::eD24UnormS8Uint;

        /// @brief Fill in construction parameters using values retrieved from the given device.
        /// This method will fill in the following fields: gi, surface and present/graphics queue properties.
        ConstructParameters & setDevice(const Device &);
    };

    /// @brief Specify the desired status of the back buffer image.
    struct BackbufferStatus {
        vk::ImageLayout        layout {};
        vk::AccessFlags        access {};
        vk::PipelineStageFlags stages {};
    };

    struct Backbuffer {
        vk::Extent2D     extent {};
        vk::Format       format {};
        vk::Image        image {};
        vk::ImageView    view {};
        Ref<Framebuffer> fb {};
        BackbufferStatus status {};
    };

    /// @brief Represents a GPU frame.
    struct Frame {
        /// @brief Index of the frame. The value will be incremented after each present.
        int64_t index = 0;

        // /// @brief Index of the frame that GPU has done all the rendering. All resources used to render this frame could be safely recycled or destroyed.
        // int64_t safeFrameIndex = -1;

        /// @brief Pointer to the backbuffer of the frame.
        /// The pointer value will be invalidated after each present.
        const Backbuffer * backbuffer;

        /// @brief The semaphore that is signaled when the last present of the current backbuffer image is done.
        /// The first rendering submission for current frame should wait for this semaphore.
        vk::Semaphore imageAvailable;

        /// @brief The semaphore that present() call uses to ensure presenting happens after all rendering is done.
        /// !!! IMPORTANT !!! : It is caller's responsibility to ensure that this semaphore is signaled and only signaled by the last rendering
        /// submission of the frame. Failing to signal this semaphore will cause present() to wait forever. On the other hand, signaling this
        /// semaphore too early could cause present() showing partially rendered frame.
        vk::Semaphore renderFinished;
    };

    /// @brief Parameters to begin the built-in render pass of the swapchain.
    struct BeginRenderPassParameters {
        /// @brief Specify the clear value for color buffer.
        vk::ClearColorValue color = vk::ClearColorValue(std::array<float, 4> {0.0f, 0.0f, 0.0f, 1.0f});

        /// @brief Specify the clear value for depth and stencil buffer.
        vk::ClearDepthStencilValue depth = vk::ClearDepthStencilValue(1.0f, 0);

        /// @brief Specify the current status of the back buffer image.
        /// This is for the cmdBeginRenderPass() method insert proper barriers to transition the image to the desired layout for the render pass.
        /// If the back buffer is already in vk::ImageLayout::eColorAttachmentOptimal layout, then no barrier will be inserted.
        /// When built-in render pass ends, the back buffer image will be automatically transitioned into status suitable for present().
        BackbufferStatus backbufferStatus = {vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits::eMemoryRead, vk::PipelineStageFlagBits::eBottomOfPipe};

        BeginRenderPassParameters & setColorF(float r, float g, float b, float a = 1.0f) {
            color = vk::ClearColorValue(std::array<float, 4> {r, g, b, a});
            return *this;
        }

        BeginRenderPassParameters & setDepth(float depth_, uint32_t stencil_ = 0) {
            this->depth = vk::ClearDepthStencilValue(depth_, stencil_);
            return *this;
        }
    };

    /// @brief Specify parameters to call present().
    struct PresentParameters {
        /// @brief Specify the current status of the back buffer image when calling present().
        /// The present() function will insert proper barrier to transit the current back buffer image into VK_IMAGE_LAYOUT_PRESENT_SRC_KHR layouy.
        /// If the back buffer image is alreadh in VK_IMAGE_LAYOUT_PRESENT_SRC_KHR layout, then no barrier will be inserted.
        BackbufferStatus backbufferStatus = {vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits::eMemoryRead, vk::PipelineStageFlagBits::eBottomOfPipe};
    };

    Swapchain(const ConstructParameters &);

    ~Swapchain();

    const RenderPass & renderPass() const;

    void cmdBeginBuiltInRenderPass(vk::CommandBuffer, const BeginRenderPassParameters &);

    /// @brief End the built-in render pass.
    /// After built-in render pass ends, the back buffer image will be automatically transitioned into status suitable for present(). You can check the
    /// actual value of the status via currentFrame().backbuffer->status.
    void cmdEndBuiltInRenderPass(vk::CommandBuffer);

    /// @brief Get the current frame information.
    /// The returned value will be invalidated after each present. So you have to call this function after each present to get the latest frame information.
    /// Referencing values returned from previous frames is prohibited and will cause undefined behavior.
    const Frame & currentFrame() const;

    /// @brief Present the current frame. Also do the internal bookkeeping to switch to next frame.
    /// The post-present status of the back buffer image is stored in currentFrame().backbuffer->status.
    void present(const PresentParameters &);

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// Extensible structure chain used by Vulkan API.
struct StructureChain {
    /// data buffer that stores the VK feature structure.
    std::vector<uint8_t> buffer;

    /// construct new feature
    template<typename T>
    StructureChain(const T & feature) {
        buffer.resize(sizeof(feature));
        memcpy(buffer.data(), &feature, sizeof(feature));
    }

    template<typename T>
    T * reset() {
        buffer.resize(sizeof(T));
        return (T *) buffer.data();
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Misc. classes for future use.

class RenderLoop : public Root {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        uint32_t maxPendingGPUFrames = 1;
    };

    RenderLoop(const ConstructParameters &);
};

class Instance;

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkDevice
class Device {
public:
    RVI_NO_COPY(Device);
    RVI_NO_MOVE(Device);

    /// Define level of validation on Vulkan error.
    enum Validation {
        VALIDATION_DISABLED = 0,
        LOG_ON_VK_ERROR,
        // LOG_ON_VK_ERROR_WITH_CALL_STACK,
        THROW_ON_VK_ERROR,
        BREAK_ON_VK_ERROR,
    };

    enum Verbosity {
        SILENCE = 0,
        BRIEF,
        VERBOSE,
    };

    struct ConstructParameters {
        /// The API version that this device will work with. Ideally, it should be equal to the version you passed to VkInstanceCreateInfo
        /// when creating your instance. Leaving it as zero is equivalent as set to return value of vk::enumerateInstanceVersion().
        uint32_t apiVersion = 0;

        /// Handle to Vulkan instance.
        vk::Instance instance;

        /// Leave it at zero to create an headless device w/o presentation support.
        vk::SurfaceKHR surface;

        /// Specify extra extension to initialize VK device. Value indicate if the extension is required or not.
        std::map<std::string, bool> deviceExtensions;

        // /// Set to true to defer to VMA for device memory allocations.
        // bool useVmaAllocator = false;

        /// Basic device feature list defined by Vulkan 1.0
        vk::PhysicalDeviceFeatures features1;

        /// Extensible device feature list defined Vulkan 1.1
        std::vector<StructureChain> features2;

        /// Pointer of an already-built feature chain. If not empty, this will be attached after feature1 and feature2.
        void * features3 = nullptr;

        /// Set to true to create VMA allocator and store in the GlobalInfo::vmaAllocator field.
        bool enableVmaAllocator = true;

        /// Set to true to enable validation layer.
        /// \todo move to Device class.
        Validation validation = RAPID_VULKAN_ENABLE_DEBUG_BUILD ? LOG_ON_VK_ERROR : VALIDATION_DISABLED;

        /// set to false to make the creation log less verbose.
        Verbosity printVkInfo = BRIEF;

        ConstructParameters & setApiVersion(uint32_t v) {
            apiVersion = v;
            return *this;
        }

        ConstructParameters & setInstance(vk::Instance i) {
            instance = i;
            return *this;
        }

        ConstructParameters & setSurface(vk::SurfaceKHR s) {
            surface = s;
            return *this;
        }

        ConstructParameters & addDeviceExtension(const std::string & name, bool required = false) {
            deviceExtensions[name] = required;
            return *this;
        }

        /// Add new feature to the feature2 list.
        template<typename T>
        T & addFeature(const T & feature) {
            features2.emplace_back(feature);
            return *(T *) features2.back().buffer.data();
        }

        /// Set the features3 pointer.
        ConstructParameters & setFeatures3(void * p) {
            features3 = p;
            return *this;
        }

        ConstructParameters & setEnableVmaAllocator(bool b) {
            enableVmaAllocator = b;
            return *this;
        }

        ConstructParameters & setValidation(Validation v) {
            validation = v;
            return *this;
        }

        ConstructParameters & setPrintVkInfo(Verbosity v) {
            printVkInfo = v;
            return *this;
        }
    };

    Device(const ConstructParameters &);

    ~Device();

    /// Get the vulkan global info structure.
    const GlobalInfo * gi() const { return &_gi; }

    /// The surface that this device is created for. Could be null if the device is headless.
    vk::SurfaceKHR surface() const { return _cp.surface; }

    /// The general purpose graphics queue that is able to do everything: graphics, compute and transfer. Should always be available.
    CommandQueue * graphics() const { return _graphics; }

    /// The presentation queue. Could be null if the device is headless. Could be same as the graphics queue, if the device does not support separate
    /// present queue.
    CommandQueue * present() const { return _present; }

    /// the async compute queue. could be null if the device does not support async compute.
    CommandQueue * compute() const { return _compute; }

    /// the async transfer queue. could be null if the device does not support async transfer.
    CommandQueue * transfer() const { return _transfer; }

    bool separatePresentQueue() const { return _present != _graphics; }

    // VkResult waitIdle() const { return _vgi.device ? threadSafeDeviceWaitIdle(_vgi.device) : VK_SUCCESS; }

    vk::Device handle() const { return _gi.device; }

    operator vk::Device() const { return _gi.device; }

    operator VkDevice() const { return _gi.device; }

    vk::Device operator->() const { return _gi.device; }

private:
    ConstructParameters         _cp;
    GlobalInfo                  _gi {};
    vk::DebugReportCallbackEXT  _debugReport {};
    bool                        _lost {}; // if the device has lost.
    std::vector<CommandQueue *> _queues;  // one for each queue family
    CommandQueue *              _graphics = nullptr;
    CommandQueue *              _compute  = nullptr;
    CommandQueue *              _transfer = nullptr;
    CommandQueue *              _present  = nullptr;

private:
    VkBool32 debugCallback(vk::DebugReportFlagsEXT, vk::DebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *, const char *);

    static VkBool32 VKAPI_PTR staticDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location,
                                                  int32_t messageCode, const char * prefix, const char * message, void * userData) {
        auto self = (Device *) userData;
        return self->debugCallback((vk::DebugReportFlagsEXT) flags, (vk::DebugReportObjectTypeEXT) objectType, object, location, messageCode, prefix, message);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkInstance
class Instance {
public:
    struct ConstructParameters {
        /// @brief Optional parameter to specify which version of the API you want to use to create the instance.
        /// Leaving it as zero means using the highest available version.
        uint32_t apiVersion = 0;

        /// Specify extra layers to initialize VK instance. The 2nd value indicates if the layer is required or not.
        /// We have to use vector instead of map here, because layer loading order matters.
        std::vector<std::pair<std::string, bool>> layers {};

        /// Specify extra extension to initialize VK instance. Value indicate if the extension is required or not.
        std::map<std::string, bool> instanceExtensions {};

        /// structure chain passed to VkInstanceCreateInfo::pNext
        std::vector<StructureChain> instanceCreateInfo {};

        /// Set to true to enable validation layers and extensions.
        bool validation = RAPID_VULKAN_ENABLE_DEBUG_BUILD;

        /// Creation log output verbosity
        Device::Verbosity printVkInfo = Device::BRIEF;

        /// Define custom function pointer to load Vulkan function pointers. Set to null to use the built-in one.
        /// Ignored when RAPID_VULKAN_ENABLE_LOADER is not 1.
        PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;

        ConstructParameters & addExtensions(bool required, const char * const * exts, size_t count = 0) {
            if (0 == count) {
                for (const char * const * p = exts; *p; ++p) instanceExtensions[*p] = required;
            } else {
                for (size_t i = 0; i < count; ++i) instanceExtensions[exts[i]] = required;
            }
            return *this;
        }

        ConstructParameters & addExtensions(bool required, vk::ArrayProxy<const char * const> exts) {
            for (const auto & e : exts)
                if (e) instanceExtensions[e] = required;
            return *this;
        }

        ConstructParameters & addExtensions(bool required, vk::ArrayProxy<const std::string> exts) {
            for (const auto & e : exts)
                if (!e.empty()) instanceExtensions[e] = required;
            return *this;
        }
    };

    Instance(ConstructParameters);

    ~Instance();

    const ConstructParameters & cp() const { return _cp; }

    vk::Instance handle() const { return _instance; }

    operator vk::Instance() const { return _instance; }

    operator VkInstance() const { return _instance; }

    vk::Instance operator->() const { return _instance; }

    /// Return a device construct parameter that works with this instance.
    Device::ConstructParameters dcp() const {
        Device::ConstructParameters r;
        r.apiVersion = cp().apiVersion;
        r.instance   = handle();
        return r;
    }

private:
    ConstructParameters _cp;
    vk::Instance        _instance {};
#if RAPID_VULKAN_ENABLE_LOADER
#if 1 != VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#error \
    "rapid-vulkan does not support static dispatch loader yet. When RAPID_VULKAN_ENABLE_LOADER is set to 1, VULKAN_HPP_DISPATCH_LOADER_DYNAMIC must be set to 1 as well."
#endif
    vk::DynamicLoader _loader;
#endif
};

} // namespace RAPID_VULKAN_NAMESPACE

#endif // RAPID_VULKAN_H

#ifdef RAPID_VULKAN_IMPLEMENTATION
#include "rapid-vulkan.cpp"
#endif
