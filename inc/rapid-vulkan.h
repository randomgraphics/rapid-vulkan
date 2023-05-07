#pragma once

#ifndef RAPID_VULKAN_NAMESPACE
    #define RAPID_VULKAN_NAMESPACE rapid_vulkan
#endif

/// Define this macro to 0/false, if you want to use your own Vulkan loader. By default, rapid-vulkan uses Volk.h that
/// comes with Vulkan SDK
#ifndef RAPID_VULKAN_INCLUDE_VOLK
    #define RAPID_VULKAN_INCLUDE_VOLK 1
#endif

#ifndef RAPID_VULKAN_ASSERT
    #define RAPID_VULKAN_ASSERT assert
#endif

#ifndef RAPID_VULKAN_LOG_ERROR
    #define RAPID_VULKAN_LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef RAPID_VULKAN_THROW
    #define RAPID_VULKAN_THROW(...) throw std::runtime_error(__VA_ARGS__)
#endif

#if RAPID_VULKAN_INCLUDE_VOLK
    #include <Volk/volk.h>
#endif

#include <vulkan/vulkan.hpp>

#include <cassert>
#include <vector>
#include <string>
#include <cstdio>
#include <unordered_map>

/// RVI stands for Rapid Vulkan Implementation. Macros started with this prefix are implementation details.
#define RVI_NO_COPY(T)                 \
    T(const T &)             = delete; \
    T & operator=(const T &) = delete;
#define RVI_NO_MOVE(T)            \
    T(T &&)             = delete; \
    T & operator=(T &&) = delete;
#define RVI_NO_COPY_NO_MOVE(T) RVI_NO_COPY(T) RVI_NO_MOVE(T)

namespace RAPID_VULKAN_NAMESPACE {

// class Device {
//     //
// };

// class Instance {
//     //
// };

struct GlobalInfo {
    const vk::AllocationCallbacks * allocator = nullptr;
    vk::Instance                    instance  = nullptr;
    vk::PhysicalDevice              physical  = nullptr;
    vk::Device                      device    = nullptr;

    template<typename T>
    void safeDestroy(T & handle) const {
        if (!handle) return;
        RAPID_VULKAN_ASSERT(device);
        if constexpr (std::is_same_v<T, vk::CommandPool>) {
            device.resetCommandPool(handle, vk::CommandPoolResetFlagBits::eReleaseResources);
            device.destroy(handle, allocator);
        } else if constexpr (std::is_same_v<T, vk::CommandBuffer>) {
            device.freeCommandBuffers(handle);
        } else {
            device.destroy(handle, allocator);
        }
        handle = nullptr;
    }
};

/// Helper function to set Vulkan opaque handle's name (VK_EXT_debug_utils).
template<typename T>
inline void setVkObjectName(vk::Device device, T handle, const char * name) {
    if (!device || !VULKAN_HPP_DEFAULT_DISPATCHER.vkSetDebugUtilsObjectNameEXT || !handle || !name) return;
    auto info = vk::DebugUtilsObjectNameInfoEXT().setObjectType(handle.objectType).setObjectHandle((uint64_t) (T::NativeType) handle).setPObjectName(name);
    device.setDebugUtilsObjectNameEXT(info);
}

/// Helper function to set Vulkan opaque handle's name (VK_EXT_debug_utils).
template<typename T>
inline void setVkObjectName(vk::Device device, T handle, std::string name) {
    setVkObjectName(device, handle, name.c_str());
}

class Buffer {
public:
    struct ConstructParameters {
        //
    };

    Buffer(const ConstructParameters &);

private:
    class Impl;
    Impl * _impl = nullptr;
};

class Image {
public:
    struct ConstructParameters {
        //
    };

private:
    class Impl;
    Impl * _impl = nullptr;
};

class Sampler {
public:
    struct ConstructParameters {
        //
    };

private:
    class Impl;
    Impl * _impl = nullptr;
};

class Shader {
public:
    static Shader EMPTY;

    struct ConstructParameters {
        const char *             name {};
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

    ~Shader();

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

class CommandBuffer {
public:
    vk::CommandBuffer handle() const { return _handle; }

protected:
    CommandBuffer()          = default;
    virtual ~CommandBuffer() = default;

    vk::CommandBuffer      _handle {};
    vk::CommandBufferLevel _level = vk::CommandBufferLevel::ePrimary;
};

class CommandQueue {
public:
    struct ConstructParameters {
        GlobalInfo gi {};
        uint32_t   family = 0; ///< queue family index
        uint32_t   index  = 0; ///< queue index within family
    };

    CommandQueue(const ConstructParameters &);
    ~CommandQueue();

    const GlobalInfo & gi() const { return _cp.gi; }
    uint32_t           family() const { return _cp.family; }
    uint32_t           index() const { return _cp.index; }
    vk::Queue          handle() const { return _handle; }

    auto begin(const char * name, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) -> CommandBuffer *;
    void submit(CommandBuffer *);
    void wait(CommandBuffer * = nullptr);

private:
    class Impl;
    ConstructParameters _cp;
    vk::Queue           _handle {};
    Impl *              _impl = nullptr;
};

class RenderLoop {
public:
    struct ConstructParameters {
        uint32_t maxPendingGPUFrames = 1;
    };

    RenderLoop(const ConstructParameters &);
};

class DescriptorPool {
    struct ConstructParameters {
        //;
    };
    DescriptorPool(const ConstructParameters &);
};

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

class PipelineLayout {
public:
    PipelineLayout(const GlobalInfo & gi, const PipelineReflection &);

    ~PipelineLayout();

    vk::ArrayProxy<vk::DescriptorSetLayout> descriptorSetLayouts() const { return _sets; }

    vk::PipelineLayout handle() const { return _handle; }

private:
    GlobalInfo                           _gi;
    std::vector<vk::DescriptorSetLayout> _sets;
    vk::PipelineLayout                   _handle;
};

class PipelineArguments {
public:
    struct ConstructParameters {
        //
    };

    PipelineArguments(const ConstructParameters &);

    void setBuffer(const std::string & name, std::shared_ptr<Buffer>);
    void setImage(const std::string & name, std::shared_ptr<Image>);
    void setSampler(const std::string & name, std::shared_ptr<Sampler>);
    void setConstant(size_t offset, size_t size, const void * data);

private:
    class Impl;
    Impl * _impl = nullptr;
};

class Pipeline {
public:
    ~Pipeline();

    auto reflect() const -> const PipelineReflection & { return _reflection; }

    virtual auto createArguments() -> std::unique_ptr<PipelineArguments> = 0;

    virtual void bind(vk::CommandBuffer, const PipelineArguments &) = 0;

protected:
    PipelineReflection              _reflection;
    std::shared_ptr<PipelineLayout> _layout;
    vk::Pipeline                    _handle {};

protected:
    Pipeline::Pipeline(vk::ArrayProxy<Shader *> shaders);
};

class GraphicsPipeline : public Pipeline {
public:
    struct ConstructParameters {
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

class ComputePipeline : public Pipeline {
public:
    struct ConstructParameters {
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

class RenderPass {
    //
};

} // namespace RAPID_VULKAN_NAMESPACE