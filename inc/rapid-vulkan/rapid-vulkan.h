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
#define RAPID_VULKAN_REVISION 1

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

/// \def RAPID_VULKAN_ASSERT
/// The runtime assert macro for debug build only. This macro has no effect when
/// RAPID_VULKAN_ENABLE_DEBUG_BUILD is 0.
#ifndef RAPID_VULKAN_ASSERT
#define RAPID_VULKAN_ASSERT(expression, ...) assert(expression)
#endif

#ifndef RAPID_VULKAN_LOG_ERROR
#define RAPID_VULKAN_LOG_ERROR(...)   \
    do {                              \
        fprintf(stderr, "[ WARN] ");  \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
    } while (false)
#endif

#ifndef RAPID_VULKAN_LOG_WARN
#define RAPID_VULKAN_LOG_WARN(...)    \
    do {                              \
        fprintf(stderr, "[ERROR] ");  \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
    } while (false)
#endif

#ifndef RAPID_VULKAN_LOG_INFO
#define RAPID_VULKAN_LOG_INFO(...)    \
    do {                              \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n");        \
    } while (false)
#endif

#ifndef RAPID_VULKAN_THROW
#define RAPID_VULKAN_THROW(...) throw std::runtime_error(__VA_ARGS__)
#endif

#if RAPID_VULKAN_ENABLE_LOADER
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

// ---------------------------------------------------------------------------------------------------------------------
// include vulkan.hpp

#ifdef VOLK_H_
// volk.h defines VK_NO_PROTOTYPES to avoid symbol conflicting with vulkan.h. But it
// causes vulkan.hpp to automatically switch to dynamic loader. So we have to undefine it,
// before including vulkan.hpp
#undef VK_NO_PROTOTYPES
#endif

#include <vulkan/vulkan.hpp>

#ifdef VOLK_H_
// Now restore it.
#define VK_NO_PROTOTYPES
#endif

// ---------------------------------------------------------------------------------------------------------------------
// include VMA header

#ifdef _MSC_VER_
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wnullability-completeness"
#endif
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
#ifdef _WIN32
#include <vma/vk_mem_alloc.h>
#else
#include <vk_mem_alloc.h>
#endif
#ifdef _WIN32
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
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
#include <string>
#include <unordered_map>
#include <tuple>

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

#define RVI_THROW(...)                                                                             \
    do {                                                                                           \
        std::stringstream ss;                                                                      \
        ss << __FILE__ << "(" << __LINE__ << "): " << RAPID_VULKAN_NAMESPACE::format(__VA_ARGS__); \
        RAPID_VULKAN_LOG_ERROR("%s", ss.str().data());                                             \
        RAPID_VULKAN_THROW(ss.str());                                                              \
    } while (false)

#if RAPID_VULKAN_ENABLE_DEBUG_BUILD
// assert is enabled only in debug build
#define RVI_ASSERT RAPID_VULKAN_ASSERT
#else
#define RVI_ASSERT(...) ((void) 0)
#endif

#define RVI_VERIFY(condition, ...)                   \
    do {                                             \
        if (!(condition)) { RVI_THROW(#condition); } \
    } while (false)

#define RVI_VK_VERIFY(condition, ...)                                        \
    do {                                                                     \
        if (VK_SUCCESS != (VkResult) (condition)) { RVI_THROW(#condition); } \
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
    const vk::AllocationCallbacks * allocator    = nullptr;
    uint32_t                        apiVersion   = 0;
    vk::Instance                    instance     = nullptr;
    vk::PhysicalDevice              physical     = nullptr;
    vk::Device                      device       = nullptr;
    VmaAllocator                    vmaAllocator = nullptr;

    template<typename T, typename... ARGS>
    void safeDestroy(T & handle, ARGS... args) const {
        if (!handle) return;
        RVI_ASSERT(device);
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
//
template<typename T>
inline constexpr T clamp(T value, const T & vmin, const T & vmax) {
    return vmin > value ? vmin : vmax < value ? vmax : value;
}

// ---------------------------------------------------------------------------------------------------------------------
// Clamp a range of [offset, offset + length) into range of [0, capacity)
/// \return The offset change.
template<typename T>
inline T clampRange(T & offset, T & length, T capacity) {
    auto begin = offset; // remember the original starting point.
    if (length > capacity) length = capacity;
    T end  = offset + length;
    offset = clamp<T>(offset, 0, capacity);
    end    = clamp<T>(end, offset, capacity);
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
    auto info =
        vk::DebugUtilsObjectNameInfoEXT().setObjectType(handle.objectType).setObjectHandle((uint64_t) (typename T::NativeType) handle).setPObjectName(name);
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

protected:
    Root(const ConstructParameters & params): _name("<no-name>"s) { setName(params.name); }

    virtual void onNameChanged(const std::string & oldName) { (void) oldName; }

private:
    friend class RefBase;
    std::string           _name;
    std::atomic<uint64_t> _ref = 0;
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
            delete p;
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

    void reset(T * t) {
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

    CommandQueue(const ConstructParameters &);
    ~CommandQueue() override;

    auto desc() const -> const Desc &;
    auto begin(const char * purpose, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) -> vk::CommandBuffer;
    void submit(vk::CommandBuffer);
    void wait(vk::CommandBuffer = {});

    auto gi() const -> const GlobalInfo * { return desc().gi; }
    auto family() const -> uint32_t { return desc().family; }
    auto index() const -> uint32_t { return desc().index; }
    auto handle() const -> vk::Queue { return desc().handle; }

private:
    class Impl;
    Impl * _impl = nullptr;
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
        const GlobalInfo *      gi;
        vk::DeviceSize          size   = 0; ///< size of the buffer in bytes.
        vk::BufferUsageFlags    usage  = {};
        vk::MemoryPropertyFlags memory = vk::MemoryPropertyFlagBits::eDeviceLocal;
        vk::MemoryAllocateFlags alloc  = {};

        ConstructParameters & setStaging() {
            usage  = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
            memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
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

    /// @brief Parameters to synchoronsly update buffer content.
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
        vk::DeviceSize offset = 0; ///< the offset of the mapped area, in bytes.
        vk::DeviceSize size   = 0; ///< the size of the mapped area, in bytes.
    };

    struct MappedResult {
        void *         data   = nullptr; ///< the mapped data buffer. If null, then the mapping failed.
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
        const GlobalInfo *      gi = nullptr;
        vk::ImageCreateInfo     info;
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

        ConstructParameters & setTiling(vk::ImageTiling t) {
            info.tiling = t;
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

    struct PixelPlane {
        vk::Format   format; ///< pixel format
        uint32_t     width;  ///< width in pixels
        uint32_t     height; ///< height in pixels
        uint32_t     pitch;  ///< size in byte of a pixel block rows (could be multiple line of pixels, for compressed texture)
        const void * pixels;
    };

    struct PixelPlaneStorage {
        vk::Format           format; ///< pixel format
        uint32_t             width;  ///< width in pixels
        uint32_t             height; ///< height in pixels
        uint32_t             pitch;  ///< size in byte of a pixel block rows (could be multiple line of pixels, for compressed texture)
        std::vector<uint8_t> pixels;
    };

    struct SetContentParameters {
        uint32_t             queueFamily = 0;
        uint32_t             queueIndex  = 0;
        vk::ImageSubresource subresource = {};
        PixelPlane           pixels;
    };

    struct ReadContentParameters {
        uint32_t             queueFamily = 0;
        uint32_t             queueIndex  = 0;
        vk::ImageSubresource subresource = {};
    };

    Image(const ConstructParameters &);

    ~Image();

    /// @brief Get basic properties of the image.
    const Desc & desc() const;

    /// @brief Synchronously set content of one subresource
    void setContent(const SetContentParameters &);

    /// @brief Synchronously read content of one subresource
    PixelPlaneStorage readContent(const ReadContentParameters &);

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkSampler
class Sampler {
public:
    struct ConstructParameters {
        //
    };

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkShaderModule
class Shader : public Root {
public:
    static Shader EMPTY;

    struct ConstructParameters : public Root::ConstructParameters {
        const GlobalInfo *       gi = nullptr;
        vk::ArrayProxy<uint32_t> spirv;
        const char *             entry = "main";

        ConstructParameters & setName(const char * v) {
            name = v;
            return *this;
        }

        ConstructParameters & setGlobalInfo(const GlobalInfo * v) {
            RVI_ASSERT(v);
            gi = v;
            return *this;
        }

        ConstructParameters & setSpirv(size_t countInUInt32, const uint32_t * data) {
            spirv = vk::ArrayProxy<uint32_t>((uint32_t) countInUInt32, data);
            return *this;
        }
    };

    Shader(const ConstructParameters &);

    ~Shader() override;

    const GlobalInfo * gi() const { return _gi; }

    vk::ShaderModule handle() const { return _handle; }

    const std::string & entry() const { return _entry; }

    vk::ArrayProxy<uint32_t> spirv() const { return _spirv; }

private:
    const GlobalInfo *    _gi = nullptr;
    vk::ShaderModule      _handle {};
    std::string           _entry;
    std::vector<uint32_t> _spirv;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkDescriptorPool
class DescriptorPool : public Root {
    struct ConstructParameters : public Root::ConstructParameters {
        //;
    };
    DescriptorPool(const ConstructParameters &);
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
/// A wrapper class for VkPipelineLayout
class PipelineLayout : public Root {
public:
    PipelineLayout(const GlobalInfo * gi, const PipelineReflection &);

    ~PipelineLayout() override;

    vk::ArrayProxy<vk::DescriptorSetLayout> descriptorSetLayouts() const { return _sets; }

    vk::PipelineLayout handle() const { return _handle; }

    const GlobalInfo * gi() const { return _gi; }

private:
    const GlobalInfo *                   _gi = nullptr;
    std::vector<vk::DescriptorSetLayout> _sets;
    vk::PipelineLayout                   _handle;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A utility class that are used to feed data to a pipeline.
class PipelineArguments : public Root {
public:
    struct ConstructParameters : Root::ConstructParameters {
        //
    };

    PipelineArguments(const ConstructParameters &);

    ~PipelineArguments() override;

    void setBuffer(const std::string & name, Ref<Buffer>);
    void setImage(const std::string & name, Ref<Image>);
    void setSampler(const std::string & name, Ref<Sampler>);
    void setConstant(size_t offset, size_t size, const void * data);

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkPipeline
class Pipeline : public Root {
public:
    ~Pipeline() override;

    auto reflect() const -> const PipelineReflection & { return _reflection; }

    virtual auto createArguments() -> Ref<PipelineArguments> = 0;

    virtual void cmdBind(vk::CommandBuffer, const PipelineArguments &) = 0;

protected:
    PipelineReflection  _reflection;
    Ref<PipelineLayout> _layout;
    vk::Pipeline        _handle {};

protected:
    Pipeline(const std::string & name, vk::ArrayProxy<const Shader *> shaders);
};

// ---------------------------------------------------------------------------------------------------------------------
/// A specialized version of pipeline that is used for graphics rendering.
class GraphicsPipeline : public Pipeline {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        const Shader & vs = Shader::EMPTY;
        const Shader & ps = Shader::EMPTY;
    };

    struct DrawParameters {
        vk::CommandBuffer cb {};
    };

    GraphicsPipeline(const ConstructParameters &);

    auto createArguments() -> Ref<PipelineArguments> override;

    void cmdBind(vk::CommandBuffer, const PipelineArguments &) override;

    void cmdDraw(vk::CommandBuffer, const DrawParameters &);

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A specialized version of pipeline that is used for compute.
class ComputePipeline : public Pipeline {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        const Shader & cs;
    };

    struct DispatchParameters {
        uint32_t width  = 1;
        uint32_t height = 1;
        uint32_t depth  = 1;
    };

    ComputePipeline(const ConstructParameters &);

    auto createArguments() -> Ref<PipelineArguments> override;

    void cmdBind(vk::CommandBuffer, const PipelineArguments &) override;

    void cmdDispatch(vk::CommandBuffer, const DispatchParameters &);

private:
    const GlobalInfo * _gi;
};

class Swapchain {
public:
    struct ConstructParameters {
        GlobalInfo     gi {};
        vk::Device     device {};
        vk::SurfaceKHR surface {};
        vk::Format     backBufferFormat = vk::Format::eB8G8R8A8Unorm;
        uint32_t       backBufferCount  = 3;
    };

    Swapchain(const ConstructParameters &);
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

class RenderPass {
    //
};

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

        /// Add new feature to the feature2 list.
        template<typename T>
        T & addFeature(const T & feature) {
            features2.emplace_back(feature);
            return *(T *) features2.back().buffer.data();
        }

        ConstructParameters() = default;

        ConstructParameters(const Instance &);
    };

    Device(const ConstructParameters &);

    ~Device();

    const GlobalInfo * gi() const { return &_gi; }

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
};

} // namespace RAPID_VULKAN_NAMESPACE

#endif // RAPID_VULKAN_H

#ifdef RAPID_VULKAN_IMPLEMENTATION
#include "rapid-vulkan.cpp"
#endif
