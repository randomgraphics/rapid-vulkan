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

#pragma once

/// \def RAPID_VULKAN_NAMESPACE
/// \brief Define the namespace of rapid-vulkan library.
#ifndef RAPID_VULKAN_NAMESPACE
    #define RAPID_VULKAN_NAMESPACE rapid_vulkan
#endif

/// \def RAPID_VULKAN_ENABLE_DEBUG_BUILD
/// Set to non-zero value to enable debug build. Disabled by default.
#ifndef RAPID_VULKAN_ENABLE_DEBUG_BUILD
    #define RAPID_VULKAN_ENABLE_DEBUG_BUILD 0
#endif

#ifndef RAPID_VULKAN_ENABLE_LOADER
    #define RAPID_VULKAN_ENABLE_LOADER 0
#endif

#ifndef RAPID_VULKAN_ASSERT
    #define RAPID_VULKAN_ASSERT assert
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

#ifdef VOLK_H_
    #undef VK_NO_PROTOTYPES
#endif

#include <vulkan/vulkan.hpp>

#ifdef VOLK_H_
    #define VK_NO_PROTOTYPES
#endif

#include <cassert>
#include <vector>
#include <map>
#include <string>
#include <cstdio>
#include <unordered_map>

/// RVI stands for Rapid Vulkan Implementation. Macros started with this prefix are reserved for internal use.
#define RVI_NO_COPY(T)     \
    T(const T &) = delete; \
    T & operator=(const T &) = delete;
#define RVI_NO_MOVE(T)    \
    T(T &&)     = delete; \
    T & operator=(T &&) = delete;
#define RVI_NO_COPY_NO_MOVE(T) RVI_NO_COPY(T) RVI_NO_MOVE(T)
#define RVI_STR(x)             RVI_STR_HELPER(x)
#define RVI_STR_HELPER(x)      #x

// If volk.h is used, check if it is too old.
#ifdef VOLK_HEADER_VERSION
    #if VOLK_HEADER_VERSION < VK_HEADER_VERSION
        #pragma message("[WARNING] VOLK_HEADER_VERSION(" RVI_STR(VOLK_HEADER_VERSION) ") is older VK_HEADER_VERSION(" RVI_STR( \
            VK_HEADER_VERSION) ")! You might see link errors of missing Vulkan symbols. Consider downgrade your Vulkan SDK to match the version of volk.h.")
    #endif
#endif

namespace RAPID_VULKAN_NAMESPACE {

using namespace std::string_literals;

// ---------------------------------------------------------------------------------------------------------------------
/// A utility class used to pass commonly used Vulkan global information around.
struct GlobalInfo {
    const vk::AllocationCallbacks * allocator = nullptr;
    vk::Instance                    instance  = nullptr;
    vk::PhysicalDevice              physical  = nullptr;
    vk::Device                      device    = nullptr;

    template<typename T, typename... ARGS>
    void safeDestroy(T & handle, ARGS... args) const {
        if (!handle) return;
        RAPID_VULKAN_ASSERT(device);
        if constexpr (std::is_same_v<T, vk::CommandPool>) {
            device.resetCommandPool(handle, vk::CommandPoolResetFlagBits::eReleaseResources);
            device.destroy(handle, allocator);
        } else if constexpr (std::is_same_v<T, vk::CommandBuffer>) {
            device.freeCommandBuffers(args..., {handle});
        } else {
            device.destroy(handle, allocator);
        }
        handle = nullptr;
    }
};

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
    std::string _name;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkBuffer
class Buffer : public Root {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        //
    };

    Buffer(const ConstructParameters &);

private:
    class Impl;
    Impl * _impl = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkImage and VkImageView
class Image {
public:
    struct ConstructParameters {
        //
    };

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
        GlobalInfo               gi {};
        vk::ArrayProxy<uint32_t> spirv;
        const char *             entry = "main";

        ConstructParameters & setName(const char * v) {
            name = v;
            return *this;
        }

        ConstructParameters & setGlobalInfo(const GlobalInfo & v) {
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

    const GlobalInfo & gi() const { return _gi; }

    vk::ShaderModule handle() const { return _handle; }

    const std::string & entry() const { return _entry; }

    vk::ArrayProxy<uint32_t> spirv() const { return _spirv; }

private:
    GlobalInfo            _gi;
    vk::ShaderModule      _handle {};
    std::string           _entry;
    std::vector<uint32_t> _spirv;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkCommandBuffer
class CommandBuffer : public Root {
public:
    vk::CommandBuffer handle() const { return _handle; }

protected:
    CommandBuffer(const Root::ConstructParameters & cp): Root(cp) {}
    ~CommandBuffer() override = default;

    vk::CommandBuffer      _handle {};
    vk::CommandBufferLevel _level = vk::CommandBufferLevel::ePrimary;
};

// ---------------------------------------------------------------------------------------------------------------------
/// A wrapper class for VkQueue
class CommandQueue : public Root {
public:
    struct ConstructParameters : public Root::ConstructParameters {
        GlobalInfo gi {};
        uint32_t   family = 0; ///< queue family index
        uint32_t   index  = 0; ///< queue index within family
    };

    CommandQueue(const ConstructParameters &);
    ~CommandQueue() override;

    const GlobalInfo & gi() const { return _gi; }
    uint32_t           family() const { return _family; }
    uint32_t           index() const { return _index; }
    vk::Queue          handle() const { return _handle; }

    auto begin(const char * name, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) -> CommandBuffer *;
    void submit(CommandBuffer *);
    void wait(CommandBuffer * = nullptr);

private:
    class Impl;
    GlobalInfo _gi {};
    uint32_t   _family = 0; ///< queue family index
    uint32_t   _index  = 0; ///< queue index within family
    vk::Queue  _handle {};
    Impl *     _impl = nullptr;
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
    PipelineLayout(const GlobalInfo & gi, const PipelineReflection &);

    ~PipelineLayout() override;

    vk::ArrayProxy<vk::DescriptorSetLayout> descriptorSetLayouts() const { return _sets; }

    vk::PipelineLayout handle() const { return _handle; }

    const GlobalInfo & gi() const { return _gi; }

private:
    GlobalInfo                           _gi;
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

    void setBuffer(const std::string & name, std::shared_ptr<Buffer>);
    void setImage(const std::string & name, std::shared_ptr<Image>);
    void setSampler(const std::string & name, std::shared_ptr<Sampler>);
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

    virtual auto createArguments() -> std::unique_ptr<PipelineArguments> = 0;

    virtual void bind(vk::CommandBuffer, const PipelineArguments &) = 0;

protected:
    PipelineReflection              _reflection;
    std::shared_ptr<PipelineLayout> _layout;
    vk::Pipeline                    _handle {};

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

    auto createArguments() -> std::unique_ptr<PipelineArguments> override;

    void bind(vk::CommandBuffer, const PipelineArguments &) override;

    void draw(vk::CommandBuffer, const DrawParameters &);

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

    auto createArguments() -> std::unique_ptr<PipelineArguments> override;

    void bind(vk::CommandBuffer, const PipelineArguments &) override;

    void dispatch(vk::CommandBuffer, const DispatchParameters &);

private:
    GlobalInfo _gi;
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
        /// pointer to Vulkan instance.
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
    };

    Device(ConstructParameters);

    ~Device();

    const ConstructParameters & cp() const { return _cp; }

    const GlobalInfo & gi() const { return _gi; }

    /// The general purpose graphics queue that is able to do everything: graphics, compute and transfer. Should always be available.
    CommandQueue * graphics() const { return _graphics; }

    /// The presentation queue. Could be null if the device is headless. Could be same as the graphics queue, if the device does not support separate present
    /// queue.
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
        /// The Vulkan API version. Default is 1.2.0
        uint32_t apiVersion = VK_MAKE_VERSION(1, 2, 0);

        /// Specify extra layers to initialize VK instance. The 2nd value indicates if the layer is required or not.
        /// We have to use vector instead of map here, because layer loading order matters.
        std::vector<std::pair<std::string, bool>> layers;

        /// Specify extra extension to initialize VK instance. Value indicate if the extension is required or not.
        std::map<std::string, bool> instanceExtensions;

        /// structure chain passed to VkInstanceCreateInfo::pNext
        std::vector<StructureChain> instanceCreateInfo;

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

#ifdef RAPID_VULKAN_IMPLEMENTATION
    #include "rapid-vulkan.cpp"
#endif
