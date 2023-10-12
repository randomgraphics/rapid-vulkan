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
#define RAPID_VULKAN_HEADER_REVISION 19

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
/// \todo explain in what cases and why you might wnat to disable the built-in loader.
#ifndef RAPID_VULKAN_ENABLE_LOADER
#define RAPID_VULKAN_ENABLE_LOADER 1
#endif

/// \def RAPID_VULKAN_ENABLE_VMA
/// Set to 0 to disable VMA support. Enabled by default.
#ifndef RAPID_VULKAN_ENABLE_VMA
#define RAPID_VULKAN_ENABLE_VMA 1
#endif

/// \def RAPID_VULKAN_ENABLE_GLFW3
/// Set to 1 to enable GLFW3 interation helpers. Disabled by default.
#ifndef RAPID_VULKAN_ENABLE_GLFW3
#define RAPID_VULKAN_ENABLE_GLFW3 0
#endif

/// \def RAPID_VULKAN_GLFW3_HEADER
/// \brief When RAPID_VULKAN_ENABLE_GLFW3 is enabled, this macro defines the path and name of GLFW3 header file to include.
/// By default, it is set to <GLFW/glfw3.h>. You can override it by defining it before including rapid-vulkan.h.
#if RAPID_VULKAN_ENABLE_GLFW3
#ifndef RAPID_VULKAN_GLFW3_HEADER
#define RAPID_VULKAN_GLFW3_HEADER <GLFW/glfw3.h>
#endif
#endif

/// \def RAPID_VULKAN_THROW
/// The macro to throw runtime exception.
/// \param errorString The error string to throw. Can be std::string or const char*.
#ifndef RAPID_VULKAN_THROW
#define RAPID_VULKAN_THROW(message) throw std::runtime_error(message)
#endif

/// \def RAPID_VULKAN_BACKTRACE
/// Define custom function to retrieve current call stack and store in std::string.
/// This macro is called when rapid-vulkan encounters critical error, to help
/// quickly identify the source of the error. The default implementation does
/// nothing but return empty string.
#ifndef RAPID_VULKAN_BACKTRACE
#define RAPID_VULKAN_BACKTRACE() std::string("You have to define RAPID_VULKAN_BACKTRACE to retrieve current call stack.")
#endif

/// \def RAPID_VULKAN_LOG_ERROR
/// The macro to log error message. The default implementation prints to stderr.
/// \paam message The error message to log. The type is const char *.
#ifndef RAPID_VULKAN_LOG_ERROR
#define RAPID_VULKAN_LOG_ERROR(message) fprintf(stderr, "[ ERROR ] %s\n", message)
#endif

/// \def RVI_LOGW
/// The macro to log warning message. The default implementation prints to stderr.
/// \param message The warning message to log. The type is const char *.
#ifndef RAPID_VULKAN_LOG_WARNING
#define RAPID_VULKAN_LOG_WARNING(message) fprintf(stderr, "[WARNING] %s\n", message)
#endif

/// \def RAPID_VULKAN_LOG_INFO
/// The macro to log informational message. The default implementation prints to stdout.
/// \param message The message to log. The type is const char *.
#ifndef RAPID_VULKAN_LOG_INFO
#define RAPID_VULKAN_LOG_INFO(message) fprintf(stdout, "%s\n", message)
#endif

/// \def RAPID_VULKAN_LOG_VERBOSE
/// The macro to log verbose log. The default implementation prints to stdout.
/// \param message The message to log. The type is const char *.
#ifndef RAPID_VULKAN_LOG_VERBOSE
#define RAPID_VULKAN_LOG_VERBOSE(message) fprintf(stdout, "[VERBOSE] %s\n", message)
#endif

/// \def RAPID_VULKAN_LOG_DEBUG
/// The macro to log debug message. The macro is ignored when RAPID_VULKAN_ENABLE_DEBUG_BUILD is 0
/// \param message The message to log. The type is const char *.
#ifndef RAPID_VULKAN_LOG_DEBUG
#define RAPID_VULKAN_LOG_DEBUG(message) fprintf(stdout, "[ DEBUG ] %s\n", message)
#endif

/// \def RAPID_VULKAN_ASSERT
/// The runtime assert macro for debug build only. This macro has no effect when
/// RAPID_VULKAN_ENABLE_DEBUG_BUILD is 0.
#ifndef RAPID_VULKAN_ASSERT
#define RAPID_VULKAN_ASSERT(expression, ...)                                       \
    if (!(expression)) {                                                           \
        auto errorMessage__ = RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__);         \
        RVI_LOGE("Condition " #expression " not met. %s", errorMessage__.c_str()); \
        assert(false);                                                             \
    } else                                                                         \
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

// check for minimal required vulkan.hpp version.
#if VK_HEADER_VERSION < 211
#error "rapid-vulkan library requires Vulkan SDK version 1.3.211 or later."
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
#define VMA_DEBUG_ERROR_LOG RVI_LOGI
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
// include GLFW header if asked to do so
#if RAPID_VULKAN_ENABLE_GLFW3
#ifdef _glfw3_h_
// todo issue warnings if GLFW3 header is already included.
#else
#include RAPID_VULKAN_GLFW3_HEADER
#endif
#endif

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
#include <set>
#include <string>
#include <sstream>
#include <unordered_map>
#include <tuple>
#include <optional>
#include <algorithm>
#include <exception>
#include <functional>
#include <memory>

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

#define RVI_LOGE(...) RAPID_VULKAN_LOG_ERROR(RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__).c_str())
#define RVI_LOGW(...) RAPID_VULKAN_LOG_WARNING(RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__).c_str())
#define RVI_LOGI(...) RAPID_VULKAN_LOG_INFO(RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__).c_str())
#define RVI_LOGV(...) RAPID_VULKAN_LOG_VERBOSE(RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__).c_str())
#if RAPID_VULKAN_ENABLE_DEBUG_BUILD
#define RVI_LOGD(...) RAPID_VULKAN_LOG_DEBUG(RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__).c_str())
#else
#define RVI_LOGD(...) void(0)
#endif

#define RVI_THROW(...)                                                                                       \
    do {                                                                                                     \
        std::stringstream errorStream_;                                                                      \
        errorStream_ << __FILE__ << "(" << __LINE__ << "): " << RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__); \
        auto errorString_ = errorStream_.str();                                                              \
        RVI_LOGE("%s", errorString_.data());                                                                 \
        RAPID_VULKAN_THROW(errorString_);                                                                    \
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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union
#endif

using namespace std::string_literals;

// ---------------------------------------------------------------------------------------------------------------------
/// A utility class used to pass commonly used Vulkan global information around.
struct GlobalInfo {
    const vk::AllocationCallbacks * allocator           = nullptr;
    vk::Instance                    instance            = nullptr;
    vk::PhysicalDevice              physical            = nullptr;
    uint32_t                        apiVersion          = 0;
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
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
inline std::string
format(const char * format, ...) {
    va_list args;

    // Get the size of the buffer needed to store the formatted string.
    va_start(args, format);
    int size = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (size == -1) {
        // Error getting the size of the buffer.
        return {};
    }

    // Allocate the buffer.
    std::string buffer((size_t) size + 1, '\0');

    // Format the string.
    va_start(args, format);
    vsnprintf(&buffer[0], (size_t) size + 1, format, args);
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
inline void setVkHandleName(vk::Device device, T handle, const char * name) {
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
inline void setVkHandleName(vk::Device device, T handle, std::string name) {
    setVkHandleName(device, handle, name.c_str());
}

// ---------------------------------------------------------------------------------------------------------------------
/// @brief Helper function to insert a begin label to command buffer
inline bool cmdBeginDebugLabel(vk::CommandBuffer cmd, const char * name, const std::array<float, 4> & color = {1, 1, 1, 1}) {
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
    if (!VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginDebugUtilsLabelEXT) return false;
#else
    if (!::vkCmdBeginDebugUtilsLabelEXT) return false;
#endif
    if (!cmd || !name) return false;
    cmd.beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT().setPLabelName(name).setColor(color));
    return true;
}

inline void cmdEndDebugLabel(vk::CommandBuffer cmd) {
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
    if (!VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdEndDebugUtilsLabelEXT) return;
#else
    if (!::vkCmdEndDebugUtilsLabelEXT) return;
#endif
    if (cmd) cmd.endDebugUtilsLabelEXT();
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
/// Query an usable/default depth format of the device.
/// \param dev The physical device in question
/// \param stencil If we need stencil format. <0: don't care. 0: no. >0: required. Default value is -1.
/// \return An format that is suitble to create depth/stencil buffer. Or vk::eUndefined if failed.
vk::Format queryDepthFormat(vk::PhysicalDevice dev, int stencil = -1);

// ---------------------------------------------------------------------------------------------------------------------
/// @brief Calling vkDeviceWaitIdle() in a thread-safe manner.
void threadSafeWaitForDeviceIdle(vk::Device device);

#if RAPID_VULKAN_ENABLE_GLFW3
// ---------------------------------------------------------------------------------------------------------------------
/// @brief Helper functions to create a Vulkan surface from GLFW window.
inline vk::UniqueSurfaceKHR createGLFWSurface(vk::Instance instance, GLFWwindow * window) {
    VkSurfaceKHR surface;
    if (::glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) { RVI_THROW("failed to create window surface!"); }
    return vk::UniqueSurfaceKHR(surface, {instance});
}
#endif

// ---------------------------------------------------------------------------------------------------------------------
/// The root class of most of the other public classes in this library.
class Root {
public:
    RVI_NO_COPY_NO_MOVE(Root);

    struct ConstructParameters {
        std::string name = "<no-name>"s;
    };

    virtual ~Root() { RVI_ASSERT(_ref == 0); }

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
    void doNotDeleteOnZeroRef() { _noDeleteOnZeroRef = true; }

    /// @brief Get the reference count of the object.
    uint64_t refCount() const { return _ref; }

    /// @brief Get the total number of instances of this class.
    static uint64_t instanceCount() { return _instanceCount; }

protected:
    Root(const ConstructParameters & params): _name("<no-name>"s) {
        ++_instanceCount;
        setName(params.name);
    }

    virtual void onNameChanged(const std::string & oldName) {
        (void) oldName;
        --_instanceCount;
    }

private:
    friend class RefBase;
    std::string                         _name;
    mutable std::atomic<uint64_t>       _ref               = 0;
    bool                                _noDeleteOnZeroRef = false;
    inline static std::atomic<uint64_t> _instanceCount     = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
/// Base class of Ref class.
class RefBase {
protected:
    RefBase() {}

    ~RefBase() {}

    static void addRef(const Root * p) {
        RVI_ASSERT(p);
        ++p->_ref;
    }

    static void release(const Root * p) {
        RVI_ASSERT(p);
        if (1 == p->_ref.fetch_sub(1)) {
            RVI_ASSERT(0 == p->_ref);
            if (!p->_noDeleteOnZeroRef) delete p;
        }
    }
};

// ---------------------------------------------------------------------------------------------------------------------
/// Reference counter of any class inherited from Root. Because it relies on the intrinsic counter variable in the Root
/// class, it is much more efficient and memory friendly than thd std::shared_ptr.
template<typename T>
class Ref : public RefBase {
public:
    /// @brief Create a Ref instance from an object instance allocated on stack.
    static Ref fromStack(T & t) {
        t.doNotDeleteOnZeroRef();
        return Ref(t);
    }

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
        _ptr = rhs.get();
        if (_ptr) addRef(_ptr);
    }

    /// copy constructor from compatible type
    template<typename T2>
    Ref(const Ref<T2> & rhs) {
        _ptr = rhs.get();
        if (_ptr) addRef(_ptr);
    }

    /// move constructor
    Ref(Ref && rhs) { _ptr = rhs.detach(); }

    /// move constructor from compatible type
    template<typename T2>
    Ref(Ref<T2> && rhs) {
        _ptr = rhs.detach();
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

    T * detach() {
        auto p = _ptr;
        _ptr   = nullptr;
        return p;
    }

    T * get() const { return (T *) _ptr; }

    /// get address of the underlying pointer
    T * const * addr() const {
        RVI_ASSERT(_ptr);
        return &_ptr;
    }

    /// copy operator
    Ref & operator=(const Ref & rhs) {
        auto p = rhs.get();
        if (_ptr == p) return *this;
        if (p) addRef(p);
        if (_ptr) release(_ptr);
        _ptr = p;
        return *this;
    }

    /// copy operator from compatible type
    template<typename T2>
    Ref & operator=(const Ref<T2> & rhs) {
        auto p = rhs.get();
        if (_ptr == p) return *this;
        if (p) addRef(p);
        if (_ptr) release(_ptr);
        _ptr = p;
        return *this;
    }

    /// move operator
    Ref & operator=(Ref && rhs) {
        if (this != &rhs) {
            if (_ptr) release(_ptr);
            _ptr = rhs.detach();
        }
        return *this;
    }

    /// move operator from compatible type
    template<typename T2>
    Ref & operator=(Ref<T2> && rhs) {
        if (this != &rhs) {
            if (_ptr) release(_ptr);
            _ptr = rhs.detach();
        }
        return *this;
    }

    /// comparison operator
    bool operator==(const Ref & rhs) const { return _ptr == rhs.get(); }

    /// comparison operator for compatible type
    template<typename T2, std::enable_if_t<std::is_convertible_v<T2, T>>>
    bool operator==(const Ref<T2> & rhs) const {
        return _ptr == rhs.get();
    }

    /// comparison operator for nullptr
    bool operator==(std::nullptr_t) { return _ptr == nullptr; }

    /// comparison operator
    bool operator!=(const Ref & rhs) const { return _ptr != rhs.get(); }

    /// comparison operator for compatible type
    template<typename T2>
    bool operator!=(const Ref<T2> & rhs) const {
        return _ptr != rhs.get();
    }

    /// comparison operator for nullptr
    bool operator!=(std::nullptr_t) { return _ptr != nullptr; }

    /// comparison operator
    bool operator<(const Ref & rhs) const { return _ptr < rhs.get(); }

    /// comparison operator
    template<typename T2>
    bool operator<(const Ref<T2> & rhs) const {
        return _ptr < rhs.get();
    }

    /// comparison operator
    bool operator<(std::nullptr_t) const { return _ptr < (T *) nullptr; }

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

class CommandQueue;

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

        SetContentParameters & setQueue(uint32_t family, uint32_t index) {
            queueFamily = family;
            queueIndex  = index;
            return *this;
        }

        SetContentParameters & setQueue(const CommandQueue &);

        SetContentParameters & setData(const void * data_, vk::DeviceSize size_) {
            data = data_;
            size = size_;
            return *this;
        }

        template<typename T>
        SetContentParameters & setData(vk::ArrayProxy<const T> data_) {
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

        ReadParameters & setQueue(uint32_t family, uint32_t index) {
            queueFamily = family;
            queueIndex  = index;
            return *this;
        }

        ReadParameters & setQueue(const CommandQueue &);

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

        Map(Buffer & buffer_, vk::DeviceSize offset_ = 0, vk::DeviceSize size_ = vk::DeviceSize(-1)) {
            RVI_ASSERT(empty());
            auto mapped = buffer_.map({offset_, size_});
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
    auto setContent(const SetContentParameters &) -> Buffer &;
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

        ConstructParameters & setDepth(size_t w, size_t h, vk::Format f) {
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
    };

    struct Desc {
        vk::Image               handle         = {};
        vk::ImageType           type           = vk::ImageType::e2D;
        vk::Format              format         = vk::Format::eR8G8B8A8Unorm;
        vk::Extent3D            extent         = {1, 1, 1};
        uint32_t                mipLevels      = 1;
        uint32_t                arrayLayers    = 1;
        vk::SampleCountFlagBits samples        = vk::SampleCountFlagBits::e1;
        bool                    cubeCompatible = false;

        bool isCube() const { return vk::ImageType::e2D == type && extent.width == extent.height && 1 == extent.depth && 6 == arrayLayers && cubeCompatible; }

        bool isCubeOrCubeArray() const {
            return vk::ImageType::e2D == type && extent.width == extent.height && 1 == extent.depth && 6 <= arrayLayers && 0 == (arrayLayers % 6) &&
                   cubeCompatible;
        }
    };

    struct ImportParameters : Root::ConstructParameters {
        const GlobalInfo * gi = nullptr;
        Desc               desc {};
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

        SetContentParameters & setQueue(uint32_t family, uint32_t index) {
            queueFamily = family;
            queueIndex  = index;
            return *this;
        }

        SetContentParameters & setQueue(const CommandQueue &);

        SetContentParameters & setPixels(const void * p) {
            pixels = p;
            return *this;
        }
    };

    struct ReadContentParameters {
        uint32_t queueFamily = 0;
        uint32_t queueIndex  = 0;

        ReadContentParameters & setQueue(uint32_t family, uint32_t index) {
            queueFamily = family;
            queueIndex  = index;
            return *this;
        }

        ReadContentParameters & setQueue(const CommandQueue &);
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
    static vk::ImageAspectFlags determineImageAspect(vk::Format format, vk::ImageAspectFlags hint = vk::ImageAspectFlagBits::eNoneKHR);

    /// @brief Construct an image from scratch
    Image(const ConstructParameters &);

    /// @brief Create an image class instance from an existing vk::Image.
    /// Note that some functionalies of the image class could be limited.
    Image(const ImportParameters &);

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
        const GlobalInfo *    gi {};
        vk::SamplerCreateInfo info {};

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
        if (_handle) setVkHandleName(_gi->device, _handle, name());
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

        template<typename T>
        ConstructParameters & setSpirv(vk::ArrayProxy<const T> blob) {
            spirv = vk::ArrayProxy<const uint32_t>((uint32_t) (blob.size() * sizeof(T) / sizeof(uint32_t)), (const uint32_t *) blob.data());
            return *this;
        }
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

    ImageSampler & setImageView(vk::ImageView v) {
        imageView = v;
        return *this;
    }

    ImageSampler & setImageLayout(vk::ImageLayout v) {
        imageLayout = v;
        return *this;
    }

    ImageSampler & setSampler(vk::Sampler v) {
        sampler = v;
        return *this;
    }

    bool operator==(const ImageSampler & rhs) const {
        if (this == &rhs) return true;
        if (imageView != rhs.imageView) return false;
        return sampler != rhs.sampler;
    }

    bool operator!=(const ImageSampler & rhs) const { return !(*this == rhs); }
};

// ---------------------------------------------------------------------------------------------------------------------
/// A utility class that describes parameter layout of a pipeline object.
struct PipelineReflection {
    /// @brief Represents one descriptor or descriptor array.
    struct Descriptor {
        /// @brief names of the shader variable.
        /// The whole structure is considered empty/invalid, if names set is empty.
        std::set<std::string> names;

        /// @brief The descriptor binding information.
        /// If the descriptor count is empty, then the whole structure is considered empty/invalid.
        vk::DescriptorSetLayoutBinding binding;

        /// @TODO checking count against 0 does not work for array of descriptors with dynamic count.
        bool empty() const { return names.empty() || 0 == binding.descriptorCount; }
    };

    /// Collection of descriptors in one set.
    typedef std::vector<Descriptor> DescriptorSet;

    /// Collection of descriptor sets indexed by the set index.
    typedef std::vector<DescriptorSet> DescriptorLayout;

    struct Constant {
        uint32_t begin = (uint32_t) -1;
        uint32_t end   = 0;
        // TODO: add push constant name information.

        bool empty() const { return begin >= end; }
    };

    /// Collection of push constants for each shader stage.
    typedef std::map<vk::ShaderStageFlagBits, Constant> ConstantLayout;

    /// Properties of vertex shader input.
    struct VertexShaderInput {
        vk::Format  format = vk::Format::eUndefined;
        std::string shaderVariable; ///< name of the shader variable.
    };

    /// Collection of vertex shader input. Key is input location.
    typedef std::map<uint32_t, VertexShaderInput> VertexLayout;

    std::string      name; ///< name of the program that this reflect is from. this field is for logging and debugging.
    DescriptorLayout descriptors;
    ConstantLayout   constants;
    VertexLayout     vertex;

    PipelineReflection() {}
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkPipeline. Immutable after being created. Safe to visit from multiple threads.
class Pipeline : public Root {
public:
    ~Pipeline() override;

    vk::PipelineBindPoint bindPoint() const;

    vk::Pipeline handle() const;

    vk::PipelineLayout layout() const;

    const PipelineReflection & reflection() const;

protected:
    Pipeline(const std::string & name, vk::PipelineBindPoint bindPoint, vk::ArrayProxy<const Shader * const> shaders);

    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// Wrapper of graphics pipeline object.
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
        std::map<vk::DynamicState, uint64_t>               dynamic {};
        vk::Pipeline                                       baseHandle {};
        int32_t                                            baseIndex {};

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

        /// @brief Add a vertex attribute, in order of location.
        /// The first call to this method adds a vertex attribute for location 0. The second call adds a vertex attribute for location 1, and so on.
        ConstructParameters & addVertexAttribute(size_t binding, size_t offset, vk::Format format) {
            vk::VertexInputAttributeDescription desc {};
            desc.binding  = (uint32_t) binding;
            desc.location = (uint32_t) va.size();
            desc.offset   = (uint32_t) offset;
            desc.format   = format;
            va.push_back(desc);
            return *this;
        }

        /// @brief Add a vertex buffer, in order of vertex buffer binding index.
        /// The first call to this method or addInstanceBuffer() adds a vertex buffer for binding 0. The second call to them adds a vertex buffer for binding 1,
        /// and so on.
        ConstructParameters & addVertexBuffer(uint32_t stride) {
            vk::VertexInputBindingDescription desc;
            desc.binding   = (uint32_t) vb.size();
            desc.stride    = stride;
            desc.inputRate = vk::VertexInputRate::eVertex;
            vb.push_back(desc);
            return *this;
        }

        /// @brief Add an instance buffer, in order of vertex buffer binding index.
        /// The first call to this method or addVertexBuffer() adds a vertex buffer for binding 0. The second call to them adds a vertex buffer for binding 1,
        /// and so on.
        ConstructParameters & addInstanceBuffer(uint32_t stride) {
            vk::VertexInputBindingDescription desc;
            desc.binding   = (uint32_t) vb.size();
            desc.stride    = stride;
            desc.inputRate = vk::VertexInputRate::eInstance;
            vb.push_back(desc);
            return *this;
        }

        /// @brief Add a new static viewport and scissor to the pipeline.
        ConstructParameters & addStaticViewportAndScissor(int x, int y, uint32_t w, uint32_t h) {
            viewports.push_back({(float) x, (float) y, (float) w, (float) h, 0.0f, 1.0f});
            scissors.push_back(vk::Rect2D(vk::Offset2D(x, y), vk::Extent2D(w, h)));
            return *this;
        }

        /// @brief Enable dyanmic viewport. Also specify the number of viewports.
        /// @param count Specify how many viewports will be used. Minimal count is 1.
        ConstructParameters & dynamicViewport(size_t count = 1) {
            if (count < 1) count = 1;
            dynamic[vk::DynamicState::eViewport] = count;
            return *this;
        }

        /// @brief Enable dynamic scissor. Also specify the number of scissors.
        /// @param count Specify how many scissors will be used. Minimal count is 1.
        ConstructParameters & dynamicScissor(size_t count = 1) {
            if (count < 1) count = 1;
            dynamic[vk::DynamicState::eScissor] = count;
            return *this;
        }

    private:
        static constexpr vk::PipelineInputAssemblyStateCreateInfo defaultIAStates() {
            return vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleList);
        }

        static constexpr vk::PipelineRasterizationStateCreateInfo defaultRastStates() { return vk::PipelineRasterizationStateCreateInfo().setLineWidth(1.0f); }

        static const vk::PipelineColorBlendAttachmentState defaultAttachment() {
            return vk::PipelineColorBlendAttachmentState().setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                                             vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        }
    };

    struct DrawParameters {
        /// Instance count. Default value is 1.
        uint32_t instanceCount = 1;

        /// Index of the first instance. Default is 0.
        uint32_t firstInstance = 0;

        /// Index count for indexed draw. Set to 0 for non-indexed draw.
        uint32_t indexCount = 0;

        /// Vertex count for non-indexed draw. Ignored when indexCount is non-zero.
        uint32_t vertexCount = 0;

        union {
            /// Index of the first vertex for non-indexed draw.
            uint32_t firstVertex = 0;

            /// Index of the first index of the indexed draw.
            uint32_t firstIndex;
        };

        /// Vertex offset of indexed draw. Ignored for non-indexed draw.
        int32_t vertexOffset = 0;

        DrawParameters & setNonIndexed(size_t vertexCount_, size_t firstVertex_ = 0) {
            indexCount  = 0;
            vertexCount = (uint32_t) vertexCount_;
            firstVertex = (uint32_t) firstVertex_;
            return *this;
        }

        DrawParameters & setIndexed(size_t indexCount_, size_t firstIndex_ = 0, int32_t vertexOffset_ = 0) {
            indexCount   = (uint32_t) indexCount_;
            vertexCount  = 0;
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

    void cmdDraw(vk::CommandBuffer, const DrawParameters &) const;
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

    void cmdDispatch(vk::CommandBuffer, const DispatchParameters &) const;
};

// ---------------------------------------------------------------------------------------------------------------------
/// @brief A compact snapshot of the drawable object.
struct DrawPack {
    struct ConstantArgument {
        vk::ShaderStageFlags stages {};
        uint32_t             offset {};
        std::vector<uint8_t> value {};
    };

    Ref<const Pipeline> pipeline;

    std::vector<std::vector<vk::WriteDescriptorSet>> descriptors;

    std::vector<ConstantArgument> constants;

    std::vector<vk::Buffer>     vertexBuffers;
    std::vector<vk::DeviceSize> vertexOffsets;

    BufferView    indexBuffer;                        ///< Index buffer. Set to null for non-indexed draw.
    vk::IndexType indexType = vk::IndexType::eUint16; ///< Type of index. Ignored for non-indexed draw.

    union {
        GraphicsPipeline::DrawParameters    draw;     ///< Draw parameters for graphics pipeline.
        ComputePipeline::DispatchParameters dispatch; ///< Dispatch parameters for compute pipeline.
    };

    DrawPack() {}

    ~DrawPack() {}

    typedef std::function<vk::DescriptorSet(const Pipeline &, uint32_t setIndex)> DescriptorSetAllocator;

    struct RenderParameters {
        vk::Device             device {};
        DescriptorSetAllocator descriptorSetAllocator {};
        const DrawPack *       previous {};
    };

    void cmdRender(vk::CommandBuffer cb, const RenderParameters &) const;
};

// ---------------------------------------------------------------------------------------------------------------------
/// Unique identifier of a pipeline descriptor
union DescriptorIdentifier {
    uint64_t u64 = 0;
    struct {
        uint32_t set;
        uint32_t binding;
    };

    DescriptorIdentifier() = default;

    DescriptorIdentifier(uint32_t s, uint32_t b): set(s), binding(b) {}

    bool operator==(const DescriptorIdentifier & rhs) const { return u64 == rhs.u64; }

    bool operator!=(const DescriptorIdentifier & rhs) const { return u64 != rhs.u64; }

    bool operator<(const DescriptorIdentifier & rhs) const { return u64 < rhs.u64; }
};

// ---------------------------------------------------------------------------------------------------------------------
/// @brief Represent a pipeline and the full set of resources/parameters to issue a draw/dispatch call to GPU.
/// The object is not thread safe. The methods can only be used in strictly sequential manner.
class Drawable : public Root {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        Ref<const Pipeline> pipeline {};
    };

    /// @brief Construct a drawable object.
    Drawable(const ConstructParameters &);

    /// @brief Destruct the drawable object.
    ~Drawable();

    /// @brief reset the drawable back to default state.
    Drawable & reset();

    /// @brief Set value of buffer argument. Do nothing if the argument is not used by the pipeline.
    Drawable & b(DescriptorIdentifier id, vk::ArrayProxy<const BufferView>);

    /// @brief Set value of texture (image/sampler) argument. Do nothing if the argument is not used by the pipeline.
    Drawable & t(DescriptorIdentifier id, vk::ArrayProxy<const ImageSampler>);

    /// @brief Set value of sampler argument. Do nothing if the argument is not used by the pipeline.
    Drawable & s(DescriptorIdentifier id, vk::ArrayProxy<const vk::Sampler>);

    /// @brief Set value of sampler argument. Do nothing if the argument is not used by the pipeline.
    Drawable & s(DescriptorIdentifier id, vk::ArrayProxy<const Ref<const Sampler>>);

    /// @brief Set value of push constant.
    Drawable & c(size_t offset, size_t size, const void * data, vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eAll);

    /// @brief Set value of push constant.
    template<typename T>
    Drawable & c(size_t offset, vk::ArrayProxy<T> data, vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eAll) {
        return c(offset, data.size() * sizeof(T), data.data(), stages);
    }

    /// @brief Append a vertex buffer to the drawable.
    Drawable & v(vk::ArrayProxy<const BufferView>);

    /// @brief Set index buffer
    Drawable & i(const BufferView & buffer, vk::IndexType type = vk::IndexType::eUint16);

    /// @brief Set draw parameters
    Drawable & draw(const GraphicsPipeline::DrawParameters &);

    /// @brief Set dispatch parameters
    Drawable & dispatch(const ComputePipeline::DispatchParameters &);

    /// @brief Create a compat snapshot of the drawable.
    std::shared_ptr<const DrawPack> compile() const;

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkCommandBuffer
class CommandBuffer {
public:
    class Impl;

    struct SubmitParameters {
        /// The (optional) fence object to signal once the command buffers have completed execution.
        vk::Fence signalFence = {};

        /// @brief List of semaphores to wait for before executing the command buffers.
        vk::ArrayProxy<const vk::Semaphore> waitSemaphores {};

        /// @brief List of semaphores to signal once the command buffers have completed execution.
        vk::ArrayProxy<const vk::Semaphore> signalSemaphores {};
    };

    CommandBuffer(Impl * impl = nullptr): _impl(impl) {}
    CommandBuffer(const CommandBuffer & o): _impl(o._impl) {}
    ~CommandBuffer() = default;

    const std::string & name() const;

    vk::CommandBuffer handle() const;

    Impl * impl() const { return _impl; }

    bool empty() const { return _impl == nullptr; }

    bool finished() const;

    bool pending() const;

    bool recording() const;

    /// @brief Enqueue a draw pack to the queue to be rendered later.
    /// The drawable and the associated resources are considered in-use until the command buffer is dropped or finished executing on GPU.
    /// Deleting the drawable object before the command buffer is dropped or finished executing on GPU will result in undefined behavior.
    const CommandBuffer & render(const DrawPack &) const;

    /// @brief Enqueue a draw pack to the queue to be rendered later.
    CommandBuffer & render(const DrawPack &);

    CommandBuffer & operator=(const CommandBuffer & o) {
        _impl = o._impl;
        return *this;
    }

    bool operator<(const CommandBuffer & o) const { return _impl < o._impl; }

    bool operator==(const CommandBuffer & o) const { return _impl == o._impl; }

    bool operator!=(const CommandBuffer & o) const { return _impl != o._impl; }

    operator vk::CommandBuffer() const { return handle(); }

    operator VkCommandBuffer() const { return handle(); }

    operator bool() const { return _impl != nullptr; }

private:
    Impl * _impl = nullptr;
};

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
        /// @brief The command buffers to submit. The command buffers must be allocated out of this queue class.
        vk::ArrayProxy<const CommandBuffer> commandBuffers {};

        /// The (optional) fence object to signal once the command buffers have completed execution.
        vk::Fence signalFence = {};

        /// @brief List of semaphores to wait for before executing the command buffers.
        vk::ArrayProxy<const vk::Semaphore> waitSemaphores {};

        /// @brief List of semaphores to signal once the command buffers have completed execution.
        vk::ArrayProxy<const vk::Semaphore> signalSemaphores {};
    };

    /// @brief unique identifier of a GPU submission
    struct SubmissionID {
        int64_t queue {};
        int64_t index {};

        bool empty() const { return !queue || 0 == index; }

        bool newerThan(int64_t other) const { return index - other > 0; }

        bool olderThan(int64_t other) const { return index - other < 0; }

        void wait() const {
            if (empty()) return;
            auto q = (CommandQueue *) (intptr_t) queue;
            q->wait(*this);
        }

        operator bool() const { return !empty(); }
    };

    CommandQueue(const ConstructParameters &);

    ~CommandQueue() override;

    auto desc() const -> const Desc &;

    /// @brief Begin recording a command buffer.
    /// @todo should return a custom CommandBuffer class.
    CommandBuffer begin(const char * name, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);

    /// @brief Submit command buffers to the queue for asynchronous processing.
    /// After this call, all command buffer pointers are inaccessible. The caller should not use them anymore.
    /// @return A submission ID that later to check/wait for the completion of the submission. Return an empty
    /// handle on failure.
    SubmissionID submit(const SubmitParameters &);

    /// @brief Drop command buffers. Discard all contents of them.
    /// After this call, the command buffer pointers are inaccessible. The caller should not use them anymore.
    void drop(vk::ArrayProxy<const CommandBuffer>);

    /// @brief Wait for the queue to finish processing submitted commands.
    /// @param SubmissionID The submission handle to wait for. It must be returned by the submit() call of the same queue.
    /// Empty and invalid submission will be ignored.
    CommandQueue & wait(const vk::ArrayProxy<const SubmissionID> &);

    /// @brief Wait for all submitted work to finish.
    CommandQueue & waitIdle();

    auto gi() const -> const GlobalInfo * { return desc().gi; }
    auto family() const -> uint32_t { return desc().family; }
    auto index() const -> uint32_t { return desc().index; }
    auto handle() const -> vk::Queue { return desc().handle; }

    /// @brief Create another queue object that shares the same underlying queue handle.
    CommandQueue clone(const std::string & newName = {}) const {
        return CommandQueue {ConstructParameters {{newName.empty() ? name() : newName}, gi(), family(), index()}};
    }

protected:
    void onNameChanged(const std::string &) override;

private:
    class Impl;
    Impl * _impl = nullptr;
};

class Device;

// ---------------------------------------------------------------------------------------------------------------------
/// Wrapper class of swapchain object
class Swapchain : public Root {
public:
    struct DepthStencilFormat {
        enum Mode {
            /// @brief In this mode, depth/stencil buffer is disabled. The swapchain contains color buffer only.
            DISABLED,

            /// @brief Need a depth only buffer. No stencil. Use the default format of the device.
            AUTO_DEPTH_ONLY,

            /// @brief Need a depth stencil format. Use the default format of the device.
            AUTO_DEPTH_STENCIL,

            /// @brief Use user specified depth stencil buffer format.
            USER_SPECIFIED,
        };
        Mode       mode   = AUTO_DEPTH_STENCIL;
        vk::Format format = vk::Format::eUndefined;

        /// @brief can implicitly convert to vk::Format
        operator vk::Format() const { return format; }
    };

    struct ConstructParameters : public Root::ConstructParameters {
        const GlobalInfo * gi = {}; ///< The Vulkan global info object.

        /// @brief Handle of the surface to present to. Set to null to create a headless swapchain.
        vk::SurfaceKHR surface = {};

        /// @brief Must always set to a valid queue family index that is capable of doing graphics work.
        uint32_t graphicsQueueFamily = VK_QUEUE_FAMILY_IGNORED;

        /// @brief Index of the graphics queue.
        uint32_t graphicsQueueIndex = 0;

        /// @brief Family index of the present queue, or VK_QUEUE_FAMILY_IGNORED if the present queue is the same as the
        /// graphics queue. Ignored in headless mode.
        uint32_t presentQueueFamily = VK_QUEUE_FAMILY_IGNORED;

        /// @brief Index of the present queue. Ignored when the present queue is VK_QUEUE_FAMILY_IGNORED.
        uint32_t presentQueueIndex = 0;

        /// @brief Width and height of the swapchain. Set to 0 to use the surface size.
        /// If the surface is null, then the width and height must be non-zero.
        size_t width = 0;

        /// @brief Height of the swapchain. Set to 0 to use the surface size.
        /// If the surface is null, then the width and height must be non-zero.
        size_t height = 0;

        /// @brief Number of frames in flight. Must be at least 1.
        /// The more frames in flight, the more latency you'll have. But on the other hand, the GPU will be
        /// less likely to be idle.
        size_t maxFramesInFlight = 1;

        /// @brief Whether to enable vsync. Ignored when the swapchain is headless.
        bool vsync = true;

        /// @brief Specify format of backbuffers. If set to undefined, then the first supported format will be used.
        vk::Format backbufferFormat = vk::Format::eUndefined;

        /// @brief Format of the depth buffer. If not specified, then use the default depth format of the current device.
        /// Set to undefined, if you don't need depth buffer.
        DepthStencilFormat depthStencilFormat = {};

        /// @brief Fill in construction parameters using values retrieved from the given device.
        /// This method will fill in the following fields: gi, surface and present/graphics queue properties.
        ConstructParameters & setDevice(const Device &);

        /// @brief Set dimension in pixels of the swapchain.
        ConstructParameters & setDimensions(size_t width_, size_t height_) {
            width  = width_;
            height = height_;
            return *this;
        }
    };

    /// @brief Specify the desired status of the back buffer image.
    struct BackbufferStatus {
        vk::ImageLayout        layout {};
        vk::AccessFlags        access {};
        vk::PipelineStageFlags stages {};
    };

    struct Backbuffer {
        Ref<Image>       image {};
        vk::ImageView    view {};
        vk::Framebuffer  framebuffer {};
        BackbufferStatus status {};
    };

    /// @brief Represents a GPU frame.
    struct Frame {
        /// @brief Index of the frame. The value will be incremented after each present.
        uint64_t index = 0;

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
        vk::ClearColorValue clearColor = vk::ClearColorValue(std::array<float, 4> {0.0f, 0.0f, 0.0f, 1.0f});

        /// @brief Specify the clear value for depth and stencil buffer.
        vk::ClearDepthStencilValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

        /// @brief Specify the current status of the back buffer image.
        /// This is for the cmdBeginRenderPass() method insert proper barriers to transition the image to the desired layout for the render pass.
        /// If the back buffer is already in vk::ImageLayout::eColorAttachmentOptimal layout, then no barrier will be inserted.
        /// When built-in render pass ends, the back buffer image will be automatically transitioned into status suitable for present().
        BackbufferStatus backbufferStatus = {vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits::eMemoryRead, vk::PipelineStageFlagBits::eBottomOfPipe};

        BeginRenderPassParameters & setClearColorF(vk::ArrayProxy<const float> color) {
            clearColor.setFloat32({color.size() > 0 ? color.data()[0] : 0.f, color.size() > 1 ? color.data()[1] : 0.f, color.size() > 2 ? color.data()[2] : 0.f,
                                   color.size() > 3 ? color.data()[3] : 1.f});
            return *this;
        }

        BeginRenderPassParameters & setClearDepth(float depth_, uint32_t stencil_ = 0) {
            clearDepth = vk::ClearDepthStencilValue(depth_, stencil_);
            return *this;
        }
    };

    /// @brief Specify parameters to call present().
    struct PresentParameters {
        /// @brief Specify the current status of the back buffer image when calling present().
        /// The present() function will insert proper barrier to transit the current back buffer image into VK_IMAGE_LAYOUT_PRESENT_SRC_KHR layouy.
        /// If the back buffer image is already in VK_IMAGE_LAYOUT_PRESENT_SRC_KHR layout, then no barrier will be inserted.
        BackbufferStatus backbufferStatus = {vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits::eMemoryRead, vk::PipelineStageFlagBits::eBottomOfPipe};
    };

    Swapchain(const ConstructParameters &);

    ~Swapchain();

    vk::RenderPass renderPass() const;

    CommandQueue & graphics() const;

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

// ---------------------------------------------------------------------------------------------------------------------
/// \def Device A wrapper class for VkDevice

class Instance;

class Device {
public:
    RVI_NO_COPY(Device);
    RVI_NO_MOVE(Device);

    enum Verbosity {
        SILENCE = 0,
        BRIEF,
        VERBOSE,
    };

    struct ConstructParameters {
        /// Handle to Vulkan instance.
        vk::Instance instance;

        /// Leave it at zero to create an headless device w/o presentation support.
        vk::SurfaceKHR surface {};

        /// Specify extra extension to initialize VK device. Value indicate if the extension is required or not.
        std::map<std::string, bool> deviceExtensions {};

        // /// Set to true to defer to VMA for device memory allocations.
        // bool useVmaAllocator = false;

        /// Basic device feature list defined by Vulkan 1.0
        vk::PhysicalDeviceFeatures features1 {};

        /// Extensible device feature list defined Vulkan 1.1
        std::vector<StructureChain> features2 {};

        /// Pointer of an already-built feature chain. If not empty, this will be attached after feature1 and feature2.
        void * features3 = nullptr;

        /// Set to true to create VMA allocator and store in the GlobalInfo::vmaAllocator field.
        bool enableVmaAllocator = true;

        /// set to false to make the creation log less verbose.
        Verbosity printVkInfo = BRIEF;

        ConstructParameters & setInstance(vk::Instance i) {
            instance = i;
            return *this;
        }

        ConstructParameters & setSurface(vk::SurfaceKHR s) {
            surface = s;
            return *this;
        }

        ConstructParameters & addDeviceExtension(const std::string & name, bool required = true) {
            deviceExtensions[name] = required;
            return *this;
        }

        /// Add new feature to the feature2 list.
        template<typename T>
        ConstructParameters & addFeature(const T & feature) {
            features2.emplace_back(feature);
            return *this;
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

    void waitIdle() const { return threadSafeWaitForDeviceIdle(_gi.device); }

    vk::Device handle() const { return _gi.device; }

    operator vk::Device() const { return _gi.device; }

    operator VkDevice() const { return _gi.device; }

    vk::Device operator->() const { return _gi.device; }

private:
    ConstructParameters         _cp;
    GlobalInfo                  _gi {};
    std::vector<CommandQueue *> _queues; // one for each queue family
    CommandQueue *              _graphics = nullptr;
    CommandQueue *              _compute  = nullptr;
    CommandQueue *              _transfer = nullptr;
    CommandQueue *              _present  = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkInstance
class Instance {
public:
    /// Define level of validation on Vulkan error.
    enum Validation {
        VALIDATION_DISABLED = 0,
        LOG_ON_VK_ERROR,
        // LOG_ON_VK_ERROR_WITH_CALL_STACK,
        THROW_ON_VK_ERROR,
        BREAK_ON_VK_ERROR,
    };

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

        /// Set validation behavior.
        Validation validation = RAPID_VULKAN_ENABLE_DEBUG_BUILD ? LOG_ON_VK_ERROR : VALIDATION_DISABLED;

        /// Creation log output verbosity
        Device::Verbosity printVkInfo = Device::BRIEF;

        /// Define custom function pointer to load Vulkan function pointers. Set to null to use the built-in one.
        /// Ignored when RAPID_VULKAN_ENABLE_LOADER is not 1.
        PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;

        ConstructParameters & setValidation(Validation v) {
            validation = v;
            return *this;
        }

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
    vk::DebugReportCallbackEXT _debugReport {};
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace RAPID_VULKAN_NAMESPACE

namespace std {

template<>
struct hash<RAPID_VULKAN_NAMESPACE::DescriptorIdentifier> {
    size_t operator()(const RAPID_VULKAN_NAMESPACE::DescriptorIdentifier & v) const { return std::hash<uint64_t>()(v.u64); }
};

} // namespace std

#endif // RAPID_VULKAN_H

#ifdef RAPID_VULKAN_IMPLEMENTATION
#include "rapid-vulkan.cpp"
#endif
