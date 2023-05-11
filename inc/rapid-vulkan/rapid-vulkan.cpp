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

#ifndef RAPID_VULKAN_IMPLEMENTATION
#include "rapid-vulkan.h"
#endif
#include "3rd-party/spriv-reflect/spirv_reflect.c"
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <mutex>
#include <unordered_set>
#include <list>
#include <set>
#include <signal.h>

#if RAPID_VULKAN_ENABLE_LOADER
// implement the default dynamic dispatcher storage. Has to use this macro outside of any namespace.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;
#endif

#define RVI_THROW(...)                                             \
    do {                                                           \
        std::stringstream ss;                                      \
        ss << __FILE__ << "(" << __LINE__ << "): " << __VA_ARGS__; \
        RAPID_VULKAN_LOG_ERROR("", ss.str().data());               \
        RAPID_VULKAN_THROW(ss.str());                              \
    } while (false)

#define RVI_VERIFY(condition)                        \
    do {                                             \
        if (!(condition)) { RVI_THROW(#condition); } \
    } while (false)

namespace RAPID_VULKAN_NAMESPACE {

// ---------------------------------------------------------------------------------------------------------------------
//
std::vector<vk::PhysicalDevice> enumeratePhysicalDevices(vk::Instance instance) {
    return completeEnumerate<vk::PhysicalDevice>([&](uint32_t * count, vk::PhysicalDevice * data) { return instance.enumeratePhysicalDevices(count, data); });
}

// ---------------------------------------------------------------------------------------------------------------------
// This function currently selects the device with the longest extension list.
vk::PhysicalDevice selectTheMostPowerfulPhysicalDevice(vk::ArrayProxy<const vk::PhysicalDevice> phydevs) {
    size_t result = 0;
    size_t maxext = 0;
    for (size_t i = 0; i < phydevs.size(); ++i) {
        auto dev        = phydevs.data()[i];
        auto extensions = completeEnumerate<vk::ExtensionProperties>(
            [&](uint32_t * count, vk::ExtensionProperties * data) -> vk::Result { return dev.enumerateDeviceExtensionProperties(nullptr, count, data); });
        if (extensions.size() > maxext) {
            maxext = extensions.size();
            result = i;
        }
    }
    return phydevs.data()[result];
}

// ---------------------------------------------------------------------------------------------------------------------
//
std::vector<vk::ExtensionProperties> enumerateDeviceExtensions(vk::PhysicalDevice dev) {
    auto extensions = completeEnumerate<vk::ExtensionProperties>(
        [&](uint32_t * count, vk::ExtensionProperties * data) { return dev.enumerateDeviceExtensionProperties(nullptr, count, data); });
    std::sort(extensions.begin(), extensions.end(), [](const auto & a, const auto & b) -> bool { return strcmp(a.extensionName, b.extensionName) < 0; });
    return extensions;
}

// *********************************************************************************************************************
// Buffer
// *********************************************************************************************************************

class Buffer::Impl {
public:
    Impl(const ConstructParameters &) {}

    Impl(const ImportParameters &) {}

    auto desc() const -> const Desc & { return _desc; }

    void cmdWrite(const WriteParameters &) {
        //
    }

    void cmdCopy(const CopyParameters &) {
        //
    }

    void setContent(const SetContentParameters &) {
        //
    }

    auto readContent(const ReadParameters &) -> std::vector<uint8_t> {
        //
        return {};
    }

private:
    const GlobalInfo * _gi;
    Desc               _desc;
    vk::Buffer         _handle {};
    bool               _imported;
};

Buffer::Buffer(const ConstructParameters & cp): Root(cp), _impl(new Impl(cp)) {}
Buffer::Buffer(const ImportParameters & cp): Root(cp), _impl(new Impl(cp)) {}
Buffer::~Buffer() { delete _impl; }
auto Buffer::desc() const -> const Desc & { return _impl->desc(); }
void Buffer::cmdWrite(const WriteParameters & p) { return _impl->cmdWrite(p); }
void Buffer::cmdCopy(const CopyParameters & p) { return _impl->cmdCopy(p); }
void Buffer::setContent(const SetContentParameters & p) { return _impl->setContent(p); }
auto Buffer::readContent(const ReadParameters & p) -> std::vector<uint8_t> { return _impl->readContent(p); }
// *********************************************************************************************************************
// Shader
// *********************************************************************************************************************

Shader Shader::EMPTY({});

Shader::Shader(const ConstructParameters & params): Root(params), _gi(params.gi) {
    if (params.spirv.empty()) return; // Constructing an empty shader module. Not an error.
    _handle = _gi->device.createShaderModule({{}, params.spirv.size() * sizeof(uint32_t), params.spirv.data()}, _gi->allocator);
    _entry  = params.entry;
}

Shader::~Shader() { _gi->safeDestroy(_handle); }

// *********************************************************************************************************************
// Command Buffer/Pool/Queue
// *********************************************************************************************************************

class CommandBufferImpl : public CommandBuffer {
public:
    enum State {
        RECORDING,
        ENDED,
        EXECUTING,
        FINISHED,
    };

    std::list<CommandBufferImpl *>::iterator pending;

    CommandBufferImpl(const GlobalInfo * gi, uint32_t family, const std::string & name_, vk::CommandBufferLevel level): CommandBuffer({name_}), _gi(gi) {
        _pool = _gi->device.createCommandPool(vk::CommandPoolCreateInfo().setQueueFamilyIndex(family), _gi->allocator);

        vk::CommandBufferAllocateInfo info;
        info.commandPool        = _pool;
        info.level              = level;
        info.commandBufferCount = 1;
        _handle                 = _gi->device.allocateCommandBuffers(info)[0];
        _level                  = level;
        setVkObjectName(_gi->device, _handle, name());
        _handle.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    }

    virtual ~CommandBufferImpl() {
        _gi->safeDestroy(_handle, _pool);
        _gi->safeDestroy(_fence);
        _gi->safeDestroy(_semaphore);
        _gi->safeDestroy(_pool);
    }

    const GlobalInfo * gi() const { return _gi; }

    State state() const { return _state; }

    bool submit(vk::Queue queue) {
        // verify the command buffer state
        if (_state == RECORDING) {
            // end the command buffer
            handle().end();
            _state = ENDED;
        } else if (_state != ENDED) {
            RAPID_VULKAN_LOG_ERROR("[ERROR] Command buffer %s is not in RECORDING or ENDED state!", name().c_str());
            return false;
        }

        // create fence and semaphore
        RAPID_VULKAN_ASSERT(!_fence && !_semaphore);
        auto fence     = _gi->device.createFenceUnique({});
        auto semaphore = _gi->device.createSemaphoreUnique({});
        setVkObjectName(_gi->device, fence.get(), name());
        setVkObjectName(_gi->device, semaphore.get(), name());

        // submit the command buffer
        vk::SubmitInfo si;
        si.setSignalSemaphoreCount(1);
        si.setPSignalSemaphores({&semaphore.get()});
        si.setCommandBufferCount(1);
        si.setPCommandBuffers(&_handle);
        queue.submit({si}, fence.get());

        // done
        _state     = EXECUTING;
        _fence     = fence.release();
        _semaphore = semaphore.release();
        return true;
    }

    // wait for the command buffer to finish executing on GPU.
    void finish() {
        if (EXECUTING == _state) {
            RAPID_VULKAN_ASSERT(_fence);
            auto result = _gi->device.waitForFences({_fence}, true, UINT64_MAX);
            if (result != vk::Result::eSuccess) { RAPID_VULKAN_LOG_ERROR("[ERROR] Command buffer %s failed to wait for fence!", name().c_str()); }
            _state = FINISHED;
        }
    }

    /// Only called on pending command buffers and unconditionally mark it as finished.
    void setFinished() {
        RAPID_VULKAN_ASSERT(_state == EXECUTING);
        _state = FINISHED;
    }

private:
    const GlobalInfo * _gi;
    vk::CommandPool    _pool; // one pool for each command buffer for simplicity for now.
    State              _state     = RECORDING;
    vk::Fence          _fence     = nullptr;
    vk::Semaphore      _semaphore = nullptr;
};

class CommandQueue::Impl {
public:
    Impl(const ConstructParameters & params) {
        _desc.gi     = params.gi;
        _desc.family = params.family;
        _desc.index  = params.index;
        _desc.handle = params.gi->device.getQueue(params.family, params.index);
    }
    ~Impl() {}

    const Desc & desc() const { return _desc; }

    CommandBuffer * begin(const char * name, vk::CommandBufferLevel level) {
        auto lock = std::lock_guard {_mutex};
        auto p    = new CommandBufferImpl(_desc.gi, _desc.family, name, level);
        _all.insert(p);
        return p;
    }

    void submit(CommandBuffer * cb) {
        auto lock = std::lock_guard {_mutex};

        // check the incoming pointer
        auto p = promote(cb);
        if (!p) return;

        if (!p->submit(_desc.handle)) return;

        // Done. add the command bufer to the pending list.
        p->pending = _pendings.insert(_pendings.end(), p);
    }

    void wait(CommandBuffer * cb) {
        auto lock = std::lock_guard {_mutex};
        if (!cb) {
            waitIdle();
            return;
        }
        auto p = promote(cb);
        if (!p) return;
        finish(p);
    }

private:
    typedef std::unordered_set<CommandBufferImpl *> CommandBufferSet;
    typedef std::list<CommandBufferImpl *>          PendingList;

private:
    std::mutex       _mutex;
    CommandBufferSet _all;      /// All command buffers ever created. This is used to validate the incoming command buffer pointers.
    PendingList      _pendings; /// Command buffers that are currently being executed, sorted in order of submission.
    Desc             _desc;

private:
    CommandBufferImpl * promote(CommandBuffer * p) const {
        if (!p) return nullptr;
        auto it = _all.find((CommandBufferImpl *) p);
        if (it == _all.end()) {
            RAPID_VULKAN_LOG_ERROR("[ERROR] Invalid command buffer pointer: 0x%p.", p);
            return nullptr;
        }
        return *it;
    }

    void finish(CommandBufferImpl * p) {
        if (p) {
            p->finish();
            if (CommandBufferImpl::FINISHED != p->state()) {
                RAPID_VULKAN_LOG_ERROR("[ERROR] Can't finish command buffer %s: buffer was not in EXECUTING or FINISHED state.", p->name().c_str());
                return;
            }
        }

        // Mark this command buffer and all command buffers submitted before it as finished.
        auto end = p ? p->pending++ : _pendings.end();
        for (auto iter = _pendings.begin(); iter != end; ++iter) {
            (*iter)->setFinished();
            // TODO: move the command buffer to the finished list.
            // _finished.push_back(std::move(*iter));

            // Remove from all list. Destroy the command buffer.
            _all.erase(_all.find(*iter));
            delete *iter;
        }
        // Then remove them from the pending list.
        _pendings.erase(_pendings.begin(), end);
    }

    void waitIdle() {
        try {
            _desc.handle.waitIdle();
        } catch (vk::SystemError & error) { RAPID_VULKAN_LOG_ERROR(error.what()); }

        // then mark all command buffers as finished.
        finish(nullptr);
    }
};

CommandQueue::CommandQueue(const ConstructParameters & params): Root(params), _impl(new Impl(params)) {}
CommandQueue::~CommandQueue() { delete _impl; }
auto CommandQueue::desc() const -> const Desc & { return _impl->desc(); }
auto CommandQueue::begin(const char * name, vk::CommandBufferLevel level) -> CommandBuffer * { return _impl->begin(name, level); }
auto CommandQueue::submit(CommandBuffer * cb) -> void { _impl->submit(cb); }
auto CommandQueue::wait(CommandBuffer * cb) -> void { _impl->wait(cb); }

// *********************************************************************************************************************
// Pipeline Reflection
// *********************************************************************************************************************

template<typename T, typename FUNC, typename... ARGS>
static std::vector<T *> enumerateShaderVariables(SpvReflectShaderModule & module, FUNC func, ARGS... args) {
    uint32_t count  = 0;
    auto     result = func(&module, args..., &count, nullptr);
    RVI_VERIFY(result == SPV_REFLECT_RESULT_SUCCESS);
    std::vector<T *> v(count);
    result = func(&module, args..., &count, v.data());
    RVI_VERIFY(result == SPV_REFLECT_RESULT_SUCCESS);
    return v;
}

struct MergedDescriptorBinding {
    SpvReflectDescriptorBinding * binding    = nullptr;
    VkShaderStageFlags            stageFlags = 0;
};

struct MergedDescriptorSet {
    std::map<std::string, MergedDescriptorBinding> descriptors;
};

static const char * getDescriptorName(const SpvReflectDescriptorBinding * d) {
    const char * name = d->name;
    if (name && *name) return name;
    name = d->type_description->type_name;
    if (name && *name) return name;
    RVI_THROW("name is empty."); // TODO: make up a name using descriptors type and binding location.
}

static void mergeDescriptorSet(MergedDescriptorSet & merged, const SpvReflectShaderModule & module,
                               const vk::ArrayProxy<SpvReflectDescriptorBinding *> & incoming) {
    for (const auto & i : incoming) {
        const char * name = getDescriptorName(i);
        auto &       d    = merged.descriptors[name];
        if (d.binding) {
            RAPID_VULKAN_ASSERT(0 == strcmp(getDescriptorName(d.binding), name));
            // check for possible conflict
            if (d.binding->binding != i->binding)
                RAPID_VULKAN_LOG_ERROR("Shader variable %s has conflicting bindings: %d != %d", name, d.binding->binding, i->binding);
            else if (d.binding->descriptor_type != i->descriptor_type)
                RAPID_VULKAN_LOG_ERROR("Shader variable %s has conflicting types: %d != %d", name, d.binding->descriptor_type, i->descriptor_type);
        } else {
            d.binding = i;
        }
        d.stageFlags |= module.shader_stage;
    }
}

static PipelineReflection::Descriptor convertArray(const MergedDescriptorBinding & src) {
    PipelineReflection::Descriptor dst = {};
    dst.binding                        = src.binding->binding; // wtf, so many bindings ...
    dst.descriptorType                 = static_cast<vk::DescriptorType>(src.binding->descriptor_type);
    dst.descriptorCount                = 1;
    for (uint32_t i = 0; i < src.binding->array.dims_count; ++i) dst.descriptorCount *= src.binding->array.dims[i];
    dst.stageFlags = (vk::ShaderStageFlagBits) src.stageFlags;
    return dst;
}

static PipelineReflection::DescriptorSet convertSet(const MergedDescriptorSet & merged) {
    PipelineReflection::DescriptorSet set;
    for (const auto & kv : merged.descriptors) set[kv.first] = convertArray(kv.second);
    return set;
}

static PipelineReflection convertRefl(std::map<uint32_t, MergedDescriptorSet> & descriptors) {
    PipelineReflection refl;
    if (!descriptors.empty()) {
        refl.descriptors.resize(descriptors.rbegin()->first + 1);
        for (const auto & kv : descriptors) {
            RAPID_VULKAN_ASSERT(kv.first < refl.descriptors.size());
            refl.descriptors[kv.first] = convertSet(kv.second);
        }
    }
    return refl;
}

static void convertVertexInputs(PipelineReflection & refl, vk::ArrayProxy<SpvReflectInterfaceVariable *> vertexInputs) {
    for (auto i : vertexInputs) {
        auto name = std::string(i->name);
        if (name.substr(0, 3) == "gl_") continue; // skip OpenGL's reserved inputs.
        refl.vertex[name] = {i->location, (vk::Format) i->format};
    }
}

static PipelineReflection reflectShaders(const std::string & pipelineName, vk::ArrayProxy<const Shader *> shaders) {
    RAPID_VULKAN_ASSERT(!shaders.empty());

    // The first uint32_t is set index. The 2nd one is shader variable name.
    std::map<uint32_t, MergedDescriptorSet> merged;

    std::vector<SpvReflectInterfaceVariable *> vertexInputs;

    PipelineReflection::ConstantLayout constants;

    std::vector<SpvReflectShaderModule> modules;

    for (const auto & shader : shaders) {
        // Generate reflection data for a shader
        auto spirv = shader->spirv();
        if (spirv.empty()) continue;

        SpvReflectShaderModule module;
        SpvReflectResult       result = spvReflectCreateShaderModule(spirv.size() * sizeof(uint32_t), spirv.data(), &module);
        RVI_VERIFY(result == SPV_REFLECT_RESULT_SUCCESS);

        // Extract descriptor sets from each shader, then merge together.
        auto sets = enumerateShaderVariables<SpvReflectDescriptorSet>(module, spvReflectEnumerateEntryPointDescriptorSets, shader->entry().c_str());
        for (const auto & set : sets) mergeDescriptorSet(merged[set->set], module, {set->binding_count, set->bindings});

        // enumerate push constants
        auto pc = enumerateShaderVariables<SpvReflectBlockVariable>(module, spvReflectEnumeratePushConstantBlocks);
        for (const auto & c : pc) {
            if (constants.find(c->name) == constants.end()) {
                auto & range     = constants[c->name];
                range.offset     = c->offset;
                range.size       = c->size;
                range.stageFlags = (vk::ShaderStageFlags) module.shader_stage;
            } else {
                auto & range = constants[c->name];
                RAPID_VULKAN_ASSERT(range.offset == c->offset);
                RAPID_VULKAN_ASSERT(range.size == c->size);
                range.stageFlags |= (vk::ShaderStageFlags) module.shader_stage;
            }
        }

        // Enumerate vertex shader inputs
        if (module.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) {
            vertexInputs = enumerateShaderVariables<SpvReflectInterfaceVariable>(module, spvReflectEnumerateInputVariables);
        }

        // store module to module list. we can't delete it yet, because the convertRefl() function still references data in it.
        modules.push_back(module);
    }

    // Convert program descriptors
    auto refl = convertRefl(merged);

    // Convert vertex shader inputs
    convertVertexInputs(refl, vertexInputs);

    // Store push constants
    refl.constants = constants;

    // Destroy all modules.
    for (auto & m : modules) spvReflectDestroyShaderModule(&m);

    // done
    refl.name = pipelineName;
    return refl;
}

// *********************************************************************************************************************
// PipelineLayout
// *********************************************************************************************************************

PipelineLayout::PipelineLayout(const GlobalInfo * gi, const PipelineReflection & refl): Root({refl.name}), _gi(gi) {
    // create descriptor set layouts
    _sets.resize(refl.descriptors.size());
    for (size_t i = 0; i < _sets.size(); ++i) {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        std::set<uint32_t>                          occupied; // binding slot that is already occupied.
        bindings.reserve(refl.descriptors[i].size());
        for (const auto & d : refl.descriptors[i]) {
            // we have to check for redundant binding, since it is legit to delcare mutiple variables in GLSL on same binding number.
            if (occupied.find(d.second.binding) != occupied.end()) continue;
            occupied.insert(d.second.binding);
            bindings.push_back(d.second);
        }
        auto ci         = vk::DescriptorSetLayoutCreateInfo();
        ci.bindingCount = (uint32_t) bindings.size();
        ci.pBindings    = bindings.data();
        _sets[i]        = gi->device.createDescriptorSetLayout(ci, gi->allocator);
    }

    // create push constant array
    std::vector<vk::PushConstantRange> pc;
    pc.reserve(refl.constants.size());
    for (const auto & kv : refl.constants) pc.push_back(kv.second);

    // create pipeline layout
    auto ci                   = vk::PipelineLayoutCreateInfo();
    ci.setLayoutCount         = (uint32_t) _sets.size();
    ci.pSetLayouts            = _sets.data();
    ci.pushConstantRangeCount = (uint32_t) pc.size();
    ci.pPushConstantRanges    = pc.data();
    _handle                   = gi->device.createPipelineLayout(ci, gi->allocator);
    setVkObjectName(gi->device, _handle, refl.name);
}

PipelineLayout::~PipelineLayout() {
    for (auto & s : _sets) _gi->safeDestroy(s);
    _sets.clear();
    _gi->safeDestroy(_handle);
}

// *********************************************************************************************************************
// PipelineArguments
// *********************************************************************************************************************

PipelineArguments::PipelineArguments(const ConstructParameters & params): Root(params) {}

PipelineArguments::~PipelineArguments() {}

// *********************************************************************************************************************
// Pipeline
// *********************************************************************************************************************

Pipeline::Pipeline(const std::string & name, vk::ArrayProxy<const Shader *> shaders): Root({name}) {
    if (shaders.empty()) return; // empty pipeline is not an error.

    // create the shader reflection
    _reflection = reflectShaders(name, shaders);

    // create pipeline layout (TODO: use a cache to reuse layout)
    _layout = std::make_shared<PipelineLayout>(shaders.front()->gi(), _reflection);
}

Pipeline::~Pipeline() {
    if (_handle) {
        RAPID_VULKAN_ASSERT(_layout);
        _layout->gi()->safeDestroy(_handle);
    }
}

// *********************************************************************************************************************
// Compute Pipeline
// *********************************************************************************************************************

ComputePipeline::ComputePipeline(const ConstructParameters & params): Pipeline(params.name, {&params.cs}), _gi(params.cs.gi()) {
    vk::ComputePipelineCreateInfo ci;
    ci.setStage({{}, vk::ShaderStageFlagBits::eCompute, params.cs.handle(), params.cs.entry().c_str()});
    ci.setLayout(_layout->handle());
    _handle = _gi->device.createComputePipeline(nullptr, ci, _gi->allocator).value;
}

auto ComputePipeline::createArguments() -> std::unique_ptr<PipelineArguments> { return {}; }

void ComputePipeline::cmdBind(vk::CommandBuffer cb, const PipelineArguments &) { cb.bindPipeline(vk::PipelineBindPoint::eCompute, _handle); }

void ComputePipeline::cmdDispatch(vk::CommandBuffer cb, const DispatchParameters & dp) {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, _handle);
    cb.dispatch(dp.width, dp.height, dp.depth);
}

// *********************************************************************************************************************
// Device
// *********************************************************************************************************************

// ---------------------------------------------------------------------------------------------------------------------
//
VkBool32 Device::debugCallback(vk::DebugReportFlagsEXT flags, vk::DebugReportObjectTypeEXT objectType,
                               uint64_t, // object
                               size_t,   // location,
                               int32_t,  // messageCode,
                               const char * prefix, const char * message) {
    auto reportVkError = [&]() {
        if (objectType == vk::DebugReportObjectTypeEXT::eDevice && _lost) {
            // Ignore validation errors on lost device, to avoid spamming log with useless messages.
            return VK_FALSE;
        }

        auto v  = _cp.validation;
        auto ss = std::stringstream();
        ss << "[Vulkan] " << prefix << " : " << message;
        // if (v >= LOG_ON_VK_ERROR_WITH_CALL_STACK) { ss << std::endl << backtrace(false); }
        auto str = ss.str();
        RAPID_VULKAN_LOG_ERROR("%s", str.data());
        if (v == THROW_ON_VK_ERROR) {
            RVI_THROW("%s", str.data());
        } else if (v == BREAK_ON_VK_ERROR) {
            raise(5); // 5 is SIGTRAP
        }

        return VK_FALSE;
    };

    if (flags & vk::DebugReportFlagBitsEXT::eError) reportVkError();

    // treat warning as error.
    if (flags & vk::DebugReportFlagBitsEXT::eWarning) reportVkError();

    // if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    //     RAPID_VULKAN_LOG_WARN("[Vulkan] %s : %s", prefix, message);
    // }

    // if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
    //     RAPID_VULKAN_LOG_INFO("[Vulkan] %s : %s", prefix, message);
    // }

    // if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
    //     RAPID_VULKAN_LOG_INFO("[Vulkan] %s : %s", prefix, message);
    // }

    return VK_FALSE;
}

// ---------------------------------------------------------------------------------------------------------------------
//
template<typename StructureChainIterator>
static inline void * buildStructureChain(StructureChainIterator begin, StructureChainIterator end, void * extra = nullptr) {
    struct Segment {
        uint32_t type;
        void *   next = nullptr;
    };

    void * next = extra ? extra : nullptr;

    for (auto iter = begin; iter != end; ++iter) {
        auto & c = *iter;
        auto   s = (Segment *) c.buffer.data();
        s->next  = next;
        next     = s;
    }
    return next;
}

// ---------------------------------------------------------------------------------------------------------------------
// An helper structure to for easier use of vk::PhysicalDeviceFeatures2 structure
struct PhysicalDeviceFeatureList {
    /// constructor
    PhysicalDeviceFeatureList(const vk::PhysicalDeviceFeatures & features1, const std::vector<StructureChain> & features2, void * features3 = nullptr) {
        // store basic feature list
        _deviceFeatures.features = features1;

        // build extensible feature chain.
        _list.assign(features2.begin(), features2.end());
        _deviceFeatures.pNext = buildStructureChain(_list.begin(), _list.end(), features3);
    }

    // Returns the first (root) level struct in the feature list chain.
    const vk::PhysicalDeviceFeatures2 & root() const { return _deviceFeatures; }

    // Returns the first (root) level struct in the feature list chain.
    vk::PhysicalDeviceFeatures2 & root() { return _deviceFeatures; }

    /// Add new feature to the feature2 list.
    template<typename T>
    T & addFeature(const T & feature) {
        StructureChain sc(feature);
        _list.emplace_back(sc);
        auto t                = (T *) _list.back().buffer.data();
        t->pNext              = _deviceFeatures.pNext;
        _deviceFeatures.pNext = t;
        return *t;
    }

private:
    vk::PhysicalDeviceFeatures2 _deviceFeatures {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR};
    std::list<StructureChain>   _list;
};

// ---------------------------------------------------------------------------------------------------------------------
//
static std::string printVulkanVersion(uint32_t v) {
    std::stringstream ss;
    ss << "v" << VK_VERSION_MAJOR(v) << "." << VK_VERSION_MINOR(v) << "." << VK_VERSION_PATCH(v);
    return ss.str();
};

// ---------------------------------------------------------------------------------------------------------------------
//
static void printPhysicalDeviceInfo(const std::vector<vk::PhysicalDevice> & available, vk::PhysicalDevice selected, bool) {
#define PRINT_LIMIT(name) "        " #name " = " << p.limits.name << std::endl
    std::stringstream ss;
    ss << "===================================" << std::endl << "Available Vulkan physical devices :" << std::endl;
    for (const auto & d : available) {
        auto         p2 = d.getProperties2();
        const auto & p  = p2.properties;
        ss << ((d == selected) ? "  * " : "    ") << p.deviceName << std::endl
           << "        API version = " << printVulkanVersion(p.apiVersion) << std::endl
           << "        Driver version = " << printVulkanVersion(p.driverVersion)
           << std::endl
           // clang-format off
           << PRINT_LIMIT(maxBoundDescriptorSets)
           << PRINT_LIMIT(maxPerStageDescriptorSamplers)
           << PRINT_LIMIT(maxPerStageDescriptorUniformBuffers)
           << PRINT_LIMIT(maxPerStageDescriptorStorageBuffers)
           << PRINT_LIMIT(maxPerStageDescriptorSampledImages)
           << PRINT_LIMIT(maxPerStageDescriptorStorageImages)
           << PRINT_LIMIT(maxPerStageDescriptorInputAttachments)
           << PRINT_LIMIT(maxPerStageResources)
           << PRINT_LIMIT(maxDescriptorSetSamplers)
           << PRINT_LIMIT(maxDescriptorSetUniformBuffers)
           << PRINT_LIMIT(maxDescriptorSetUniformBuffersDynamic)
           << PRINT_LIMIT(maxDescriptorSetStorageBuffers)
           << PRINT_LIMIT(maxDescriptorSetStorageBuffersDynamic)
           << PRINT_LIMIT(maxDescriptorSetSampledImages)
           << PRINT_LIMIT(maxDescriptorSetStorageImages)
           << PRINT_LIMIT(maxDescriptorSetInputAttachments);
        // clang-format on
    }
    ss << std::endl;
    RAPID_VULKAN_LOG_INFO("%s", ss.str().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------
//
static void printDeviceFeatures(vk::PhysicalDevice phydev, const PhysicalDeviceFeatureList & enabled, bool verbose) {
    // retrieve physical device properties
    vk::PhysicalDeviceProperties properties = phydev.getProperties();

    // retrieve supported feature list
    vk::PhysicalDeviceFeatures supported = phydev.getFeatures();

    // print enabled feature list
    bool              none = true;
    std::stringstream ss;
    ss << "=====================================================================" << std::endl;
    ss << (verbose ? "Available" : "Enabled") << " features of the selected device : " << properties.deviceName << std::endl;
#define PRINT_FEATURE(x)                                          \
    if (supported.x) {                                            \
        if (enabled.root().features.x) {                          \
            ss << (verbose ? "  * " : "    ") << #x << std::endl; \
            none = false;                                         \
        } else if (verbose) {                                     \
            ss << "    " << #x << std::endl;                      \
            none = false;                                         \
        }                                                         \
    }
    PRINT_FEATURE(robustBufferAccess);
    PRINT_FEATURE(fullDrawIndexUint32);
    PRINT_FEATURE(imageCubeArray);
    PRINT_FEATURE(independentBlend);
    PRINT_FEATURE(geometryShader);
    PRINT_FEATURE(tessellationShader);
    PRINT_FEATURE(sampleRateShading);
    PRINT_FEATURE(dualSrcBlend);
    PRINT_FEATURE(logicOp);
    PRINT_FEATURE(multiDrawIndirect);
    PRINT_FEATURE(drawIndirectFirstInstance);
    PRINT_FEATURE(depthClamp);
    PRINT_FEATURE(depthBiasClamp);
    PRINT_FEATURE(fillModeNonSolid);
    PRINT_FEATURE(depthBounds);
    PRINT_FEATURE(wideLines);
    PRINT_FEATURE(largePoints);
    PRINT_FEATURE(alphaToOne);
    PRINT_FEATURE(multiViewport);
    PRINT_FEATURE(samplerAnisotropy);
    PRINT_FEATURE(textureCompressionETC2);
    PRINT_FEATURE(textureCompressionASTC_LDR);
    PRINT_FEATURE(textureCompressionBC);
    PRINT_FEATURE(occlusionQueryPrecise);
    PRINT_FEATURE(pipelineStatisticsQuery);
    PRINT_FEATURE(vertexPipelineStoresAndAtomics);
    PRINT_FEATURE(fragmentStoresAndAtomics);
    PRINT_FEATURE(shaderTessellationAndGeometryPointSize);
    PRINT_FEATURE(shaderImageGatherExtended);
    PRINT_FEATURE(shaderStorageImageExtendedFormats);
    PRINT_FEATURE(shaderStorageImageMultisample);
    PRINT_FEATURE(shaderStorageImageReadWithoutFormat);
    PRINT_FEATURE(shaderStorageImageWriteWithoutFormat);
    PRINT_FEATURE(shaderUniformBufferArrayDynamicIndexing);
    PRINT_FEATURE(shaderSampledImageArrayDynamicIndexing);
    PRINT_FEATURE(shaderStorageBufferArrayDynamicIndexing);
    PRINT_FEATURE(shaderStorageImageArrayDynamicIndexing);
    PRINT_FEATURE(shaderClipDistance);
    PRINT_FEATURE(shaderCullDistance);
    PRINT_FEATURE(shaderFloat64);
    PRINT_FEATURE(shaderInt64);
    PRINT_FEATURE(shaderInt16);
    PRINT_FEATURE(shaderResourceResidency);
    PRINT_FEATURE(shaderResourceMinLod);
    PRINT_FEATURE(sparseBinding);
    PRINT_FEATURE(sparseResidencyBuffer);
    PRINT_FEATURE(sparseResidencyImage2D);
    PRINT_FEATURE(sparseResidencyImage3D);
    PRINT_FEATURE(sparseResidency2Samples);
    PRINT_FEATURE(sparseResidency4Samples);
    PRINT_FEATURE(sparseResidency8Samples);
    PRINT_FEATURE(sparseResidency16Samples);
    PRINT_FEATURE(sparseResidencyAliased);
    PRINT_FEATURE(variableMultisampleRate);
    PRINT_FEATURE(inheritedQueries);
#undef PRINT_FEATURE
    if (none) ss << std::endl << "  [None]" << std::endl;
    ss << std::endl;
    RAPID_VULKAN_LOG_INFO("%s", ss.str().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------
//
static void printDeviceExtensions(vk::PhysicalDevice phydev, const std::vector<vk::ExtensionProperties> & available, const std::vector<const char *> & enabled,
                                  bool verbose) {
    // retrieve physical device properties
    auto properties = phydev.getProperties();

    std::stringstream ss;
    ss << "=====================================================================" << std::endl;
    ss << (verbose ? "Available" : "Enabled") << " extensions of the selected device: " << properties.deviceName << std::endl;
    bool none = true;
    for (size_t i = 0; i < available.size(); ++i) {
        const auto & e         = available[i];
        bool         supported = false;
        for (size_t j = 0; j < enabled.size(); ++j) {
            if (0 == strcmp(e.extensionName, enabled[j])) {
                supported = true;
                break;
            }
        }
        if (!supported && !verbose) continue;
        ss << ((supported && verbose) ? "*" : " ") << std::setw(3) << i << " : " << e.extensionName << " ( ver. " << e.specVersion << " )" << std::endl;
        none = false;
    }
    if (none) ss << std::endl << "  [None]" << std::endl;
    ss << std::endl;
    RAPID_VULKAN_LOG_INFO("%s", ss.str().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------
//
static void printAvailableQueues(vk::PhysicalDevice phydev, const std::vector<vk::QueueFamilyProperties> & queues, bool) {
    auto properties = phydev.getProperties();

    auto flags2str = [](vk::QueueFlags flags) -> std::string {
        std::stringstream ss;
        bool              empty = true;
        ss << (uint32_t) flags << " (";
        if (flags & vk::QueueFlagBits::eCompute) {
            if (empty) {
                empty = false;
            } else {
                ss << "|";
            }
            ss << "Graphics";
        }
        if (flags & vk::QueueFlagBits::eGraphics) {
            if (empty) {
                empty = false;
            } else {
                ss << "|";
            }
            ss << "Compute";
        }
        if (flags & vk::QueueFlagBits::eTransfer) {
            if (empty) {
                empty = false;
            } else {
                ss << "|";
            }
            ss << "Transfer";
        }
        if (flags & vk::QueueFlagBits::eSparseBinding) {
            if (empty) {
                empty = false;
            } else {
                ss << "|";
            }
            ss << "SparseBinding";
        }
        ss << ")";
        return ss.str();
    };

    std::stringstream ss;
    ss << "=====================================================================" << std::endl;
    ss << "Available queues on selected device : " << properties.deviceName << std::endl;
    for (size_t i = 0; i < queues.size(); ++i) {
        const auto & q = queues[i];
        auto         w = q.minImageTransferGranularity.width;
        auto         h = q.minImageTransferGranularity.height;
        auto         d = q.minImageTransferGranularity.depth;
        ss << " " << i << " : queueCount = " << q.queueCount << std::endl
           << "     minImageTransferGranularity = (" << w << "x" << h << "x" << d << ")" << std::endl
           << "     timestampValidBits = " << q.timestampValidBits << std::endl
           << "     queueFlags = " << flags2str(q.queueFlags) << std::endl;
    }
    ss << std::endl;

    RAPID_VULKAN_LOG_INFO("%s", ss.str().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------
//
static std::vector<const char *> validateExtensions(const std::vector<vk::ExtensionProperties> & available, const std::map<std::string, bool> & asked) {
    std::vector<const char *> supported;
    for (const auto & a : asked) {
        bool found = false;
        for (const auto & b : available) {
            if (a.first == b.extensionName) {
                supported.push_back(b.extensionName);
                found = true;
                break;
            }
        }
        if (found) continue;
        if (a.second) {
            RVI_THROW("Extension %s is not supported by current device.", a.first.c_str());
        } else {
            RAPID_VULKAN_LOG_WARN("Optional feature %s is not supported by the current device.", a.first.c_str());
        }
    }
    return supported;
}

// ---------------------------------------------------------------------------------------------------------------------
//
Device::Device(ConstructParameters cp): _cp(cp) {
    // check instance pointer
    RVI_VERIFY(cp.instance);
    _gi.instance = cp.instance;

    // setup debug callback
    if (cp.validation) {
        auto debugci = vk::DebugReportCallbackCreateInfoEXT()
                           .setFlags(vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning)
                           .setPfnCallback(staticDebugCallback)
                           .setPUserData(this);
        _debugReport = _gi.instance.createDebugReportCallbackEXT(debugci);
    }

    // enumerate physical devices
    auto phydevs = enumeratePhysicalDevices(_gi.instance);

    // TODO: pick the one specified by user.
    _gi.physical = selectTheMostPowerfulPhysicalDevice(phydevs);
    bool verbose = cp.printVkInfo == VERBOSE;
    if (cp.printVkInfo) printPhysicalDeviceInfo(phydevs, _gi.physical, verbose);

    // query queues
    auto families = _gi.physical.getQueueFamilyProperties();
    if (cp.printVkInfo) printAvailableQueues(_gi.physical, families, verbose);

    // setup device feature and extension
    PhysicalDeviceFeatureList   deviceFeatures(cp.features1, cp.features2, cp.features3);
    std::map<std::string, bool> askedDeviceExtensions = cp.deviceExtensions;

    // more extension
    askedDeviceExtensions[VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME] = true;

    // #if PH_ANDROID == 0
    //     if (isRenderDocPresent()) {                                                       // only add this when renderdoc is available
    //         askedDeviceExtensions[VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME] = true; // add this to allow debugging on compute shaders
    //     }
    // #endif

    // Add swapchain extension when there is a surface.
    bool presenting = false;
    if (_cp.surface) {
        askedDeviceExtensions[VK_KHR_SWAPCHAIN_EXTENSION_NAME] = true;
        presenting                                             = true;
    }

    // make sure all extensions are actually supported by the hardware.
    auto availableDeviceExtensions = enumerateDeviceExtensions(_gi.physical);
    auto enabledDeviceExtensions   = validateExtensions(availableDeviceExtensions, askedDeviceExtensions);

    // create device, one queue for each family
    float                                  queuePriority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfo;
    for (uint32_t i = 0; i < families.size(); ++i) { queueCreateInfo.emplace_back(vk::DeviceQueueCreateInfo({}, i, 1, &queuePriority)); }

    // create device
    vk::DeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.setPNext(&deviceFeatures);
    deviceCreateInfo.setQueueCreateInfos(queueCreateInfo);
    deviceCreateInfo.setPEnabledExtensionNames(enabledDeviceExtensions);
    _gi.device = _gi.physical.createDevice(deviceCreateInfo, _gi.allocator);

    // TODO: initialize device specific dispatcher.`

    //     // initialize a memory allocator for Vulkan images
    //     if (_cp.useVmaAllocator) {
    //         VmaVulkanFunctions vf {};
    //         vf.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    //         vf.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;
    //         VmaAllocatorCreateInfo ai {};
    //         ai.vulkanApiVersion = _cp.instance->cp().apiVersion;
    //         ai.physicalDevice   = _gi.physical;
    //         ai.device           = _gi.device;
    //         ai.instance         = _gi.instance;
    //         ai.pVulkanFunctions = &vf;

    // #if 0 // uncomment this section to enable vma allocation recording
    //         VmaRecordSettings vmaRecordSettings;
    //         vmaRecordSettings.pFilePath = "vmaReplay.csv";
    //         vmaRecordSettings.flags = VMA_RECORD_FLUSH_AFTER_CALL_BIT;
    //         ai.pRecordSettings = &vmaRecordSettings;
    // #endif

    //         if (askedDeviceExtensions.find(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) != askedDeviceExtensions.end()) {
    //             RAPID_VULKAN_LOG_INFO("Enable VMA allocator with buffer device address.");
    //             ai.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    //         }
    //         GN_VK_REQUIRE(vmaCreateAllocator(&ai, &_gi.vmaAllocator));
    //     }

    // print device information
    if (cp.printVkInfo) {
        printDeviceFeatures(_gi.physical, deviceFeatures, verbose);
        printDeviceExtensions(_gi.physical, availableDeviceExtensions, enabledDeviceExtensions, verbose);
    }

    // classify queue families. create command pool for each of them.
    _queues.resize(families.size());
    for (uint32_t i = 0; i < families.size(); ++i) {
        const auto & f = families[i];

        // create an submission proxy for each queue.
        auto name  = std::string("Default device queue #") + std::to_string(i);
        auto q     = new CommandQueue({name, &_gi, i, 0});
        _queues[i] = q;

        // classify all queues
        if (!_graphics && f.queueFlags & vk::QueueFlagBits::eGraphics) _graphics = q;

        if (!_compute && !(f.queueFlags & vk::QueueFlagBits::eGraphics) && (f.queueFlags & vk::QueueFlagBits::eCompute)) _compute = q;

        if (!_transfer && !(f.queueFlags & vk::QueueFlagBits::eGraphics) && !(f.queueFlags & vk::QueueFlagBits::eCompute) &&
            (f.queueFlags & vk::QueueFlagBits::eTransfer))
            _transfer = q;

        if (!_present && presenting) {
            auto supportPresenting = _gi.physical.getSurfaceSupportKHR(i, _cp.surface);
            if (supportPresenting) _present = q;
        }
    }

    RAPID_VULKAN_LOG_INFO("Vulkan device initialized.");
}

// ---------------------------------------------------------------------------------------------------------------------
//
Device::~Device() {
    for (auto q : _queues) delete q;
    _queues.clear();
    // if (_gi.vmaAllocator) vmaDestroyAllocator(_gi.vmaAllocator), _gi.vmaAllocator = nullptr;
    if (_gi.device) {
        RAPID_VULKAN_LOG_INFO("[Device] destroying device...");
        _gi.device.destroy(_gi.allocator);
        _gi.device = nullptr;
        RAPID_VULKAN_LOG_INFO("[Device] device destroyed");
    }
    if (_debugReport) {
        ((vk::Instance) _cp.instance).destroyDebugReportCallbackEXT(_debugReport);
        _debugReport = VK_NULL_HANDLE;
    }
}

// *********************************************************************************************************************
// InstanceInfo
// *********************************************************************************************************************

// ---------------------------------------------------------------------------------------------------------------------
//
struct InstanceInfo {
    struct LayerInfo {
        vk::LayerProperties                  properties;
        std::vector<vk::ExtensionProperties> extensions;
    };

    uint32_t                             version;
    std::vector<LayerInfo>               layers;
    std::vector<vk::ExtensionProperties> extensions;

    /// initialize the structure. Populate all data members.
    void init() {
        version = vk::enumerateInstanceVersion();

        auto properties = completeEnumerate<vk::LayerProperties>(
            [&](uint32_t * count, vk::LayerProperties * data) { return vk::enumerateInstanceLayerProperties(count, data); });

        layers.resize(properties.size());

        for (size_t i = 0; i < layers.size(); ++i) {
            layers[i].properties = properties[i];
            layers[i].extensions = completeEnumerate<vk::ExtensionProperties>([&](uint32_t * count, vk::ExtensionProperties * data) {
                return vk::enumerateInstanceExtensionProperties(properties[i].layerName, count, data);
            });
        }

        std::sort(layers.begin(), layers.end(), [](const auto & a, const auto & b) { return strcmp(a.properties.layerName, b.properties.layerName) < 0; });

        extensions = completeEnumerate<vk::ExtensionProperties>(
            [&](uint32_t * count, vk::ExtensionProperties * data) { return vk::enumerateInstanceExtensionProperties(nullptr, count, data); });

        std::sort(extensions.begin(), extensions.end(), [](const auto & a, const auto & b) { return strcmp(a.extensionName, b.extensionName) < 0; });
    }

    /// Check to see if certain instance layer is supported.
    bool checkLayer(const char * layer) const {
        if (!layer || !*layer) return true; // empty layer is always supported.
        auto iter = std::find_if(layers.begin(), layers.end(), [&](const LayerInfo & a) { return 0 == strcmp(a.properties.layerName, layer); });
        return iter != layers.end();
    }

    /// Check to see if certain instance extension is supported.
    bool checkExtension(const char * extension) const {
        if (!extension || !*extension) return true; // return true for empty extension.
        auto iter =
            std::find_if(extensions.begin(), extensions.end(), [&](const vk::ExtensionProperties & e) { return 0 == strcmp(e.extensionName, extension); });
        return iter != extensions.end();
    }

    /// result structure of check() method
    struct ValidatedExtensions {
        std::vector<const char *> layers;             ///< list of layers to initialize VK instance.
        std::vector<const char *> instanceExtensions; ///< list of extensions to initialize VK instance.
    };

    /// Check to ensure all requested layers and extensions are supported. Throw exception, if any of required
    /// layers/extensions are not supported.
    ValidatedExtensions validate(const std::map<const char *, bool> & layers_, std::map<const char *, bool> extensions_) const {
        ValidatedExtensions v;

        // hold list of supported extensions in a set to avoid duplicated extension names.
        std::set<const char *> available;

        auto processSupportedExtension = [&](const vk::ExtensionProperties & available) {
            // Add supported ones to v.instanceExtensions list. Then remove it from extensions_ list. So we won't check
            // the same extension twice.
            for (auto asked = extensions_.begin(); asked != extensions_.end();) {
                if (0 == strcmp(asked->first, available.extensionName)) {
                    v.instanceExtensions.push_back(asked->first);
                    // Remove "found" extensions from the list. So it won't be checked again.
                    asked = extensions_.erase(asked);
                } else {
                    ++asked;
                }
            }
        };

        // check against each layers
        for (auto l : layers_) {
            auto iter = std::find_if(layers.begin(), layers.end(), [&](const LayerInfo & a) { return 0 == strcmp(a.properties.layerName, l.first); });
            if (iter == layers.end()) {
                if (l.second) {
                    RVI_THROW("Required VK layer %s is not supported.", l.first);
                } else {
                    RAPID_VULKAN_LOG_WARN("Optional VK layer %s is not supported.", l.first);
                }
                continue;
            }
            v.layers.push_back(l.first);

            // Check against extension list of the layer.
            for (const auto & e : iter->extensions) { processSupportedExtension(e); }
        }

        // Check against each extensions
        for (const auto & e : extensions) { processSupportedExtension(e); }

        // We have gone through all available layers and extensions. All supported extensions should have been removed
        // from extensions_list. Now it is time to see if there's any unsupported but required extensions.
        for (const auto & asked : extensions_) {
            if (asked.second) {
                RVI_THROW("Required VK extension %s is not supported.", asked.first);
            } else {
                RAPID_VULKAN_LOG_WARN("Optional VK extension %s is not supported.", asked.first);
            }
        }

        // done
        return v;
    }

    std::string print(const vk::InstanceCreateInfo & ici, bool verbose = false) const {
        std::stringstream ss;

        auto isLayerEnabled = [&](const std::string & layer) -> bool {
            for (size_t i = 0; i < ici.enabledLayerCount; ++i) {
                const char * e = ici.ppEnabledLayerNames[i];
                if (layer == e) return true;
            }
            return false;
        };

        auto isExtensionEnabled = [&](const std::string & name) -> bool {
            for (size_t i = 0; i < ici.enabledExtensionCount; ++i) {
                const char * e = ici.ppEnabledExtensionNames[i];
                if (name == e) return true;
            }
            return false;
        };

        // retrieve supported API version.
        ss << "========================================" << std::endl
           << "Vulkan API version :" << std::endl
           << "  supported: " << printVulkanVersion(version) << std::endl
           << "    enabled: " << printVulkanVersion(ici.pApplicationInfo->apiVersion) << std::endl
           << "========================================" << std::endl
           << (verbose ? "Available" : "Enabled") << " Vulkan layers :" << std::endl;
        if (layers.empty()) {
            ss << "  <empty>" << std::endl;
        } else {
            for (const auto & l : layers) {
                bool enabled = isLayerEnabled(l.properties.layerName);
                if (!verbose && !enabled) continue;
                ss << ((enabled && verbose) ? "  * " : "    ") << l.properties.layerName << " ( "
                   << "v" << VK_VERSION_MAJOR(l.properties.specVersion) << "." << VK_VERSION_MINOR(l.properties.specVersion) << "."
                   << VK_VERSION_PATCH(l.properties.specVersion) << " ) : " << l.properties.description << std::endl;
                for (const auto & e : l.extensions) { ss << "                " << e.extensionName << " ( ver." << e.specVersion << " )" << std::endl; }
            }
        }
        ss << "========================================" << std::endl << (verbose ? "Available" : "Enabled") << " Instance extensions :" << std::endl;
        if (extensions.empty()) {
            ss << "  <empty>" << std::endl;
        } else {
            for (const auto & e : extensions) {
                bool enabled = isExtensionEnabled(e.extensionName);
                if (!verbose && !enabled) continue;
                ss << ((enabled && verbose) ? "  * " : "    ") << e.extensionName << " ( ver." << e.specVersion << " )" << std::endl;
            }
        }
        ss << std::endl;

        // done
        return ss.str();
    }
};

// ---------------------------------------------------------------------------------------------------------------------
//
struct PhysicalDeviceInfo {
    vk::PhysicalDeviceProperties         properties;
    std::vector<vk::ExtensionProperties> extensions;

    void query(vk::PhysicalDevice dev) {
        properties = dev.getProperties();
        extensions = completeEnumerate<vk::ExtensionProperties>(
            [&](uint32_t * count, vk::ExtensionProperties * data) -> vk::Result { return dev.enumerateDeviceExtensionProperties(nullptr, count, data); });
    }
};

// *********************************************************************************************************************
// Instance
// *********************************************************************************************************************

// ---------------------------------------------------------------------------------------------------------------------
//
Instance::Instance(ConstructParameters cp): _cp(cp) {
#if RAPID_VULKAN_ENABLE_LOADER
    auto getProcAddress = _cp.getInstanceProcAddr;
    if (!getProcAddress) getProcAddress = _loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(getProcAddress);
#endif

    InstanceInfo instanceInfo;
    instanceInfo.init();

    std::map<const char *, bool> layers;
    for (const auto & l : cp.layers) layers[l.first.c_str()] = l.second;
    if (cp.validation) {
        // add validation layer as an "optional" layer
        layers.insert({"VK_LAYER_KHRONOS_validation", false});
    }

    // setup extension list
    std::map<const char *, bool> instanceExtensions {
        {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, true}
        // {VK_KHR_SURFACE_EXTENSION_NAME, true},
    };
    if (cp.validation) {
        // Enable in-shader debug printf, if supported.
        instanceExtensions[VK_EXT_DEBUG_REPORT_EXTENSION_NAME] = false;
        instanceExtensions[VK_EXT_DEBUG_UTILS_EXTENSION_NAME]  = false;
    }
    for (const auto & e : cp.instanceExtensions) { instanceExtensions[e.first.c_str()] = e.second; }

    // make sure all required layers and extensions are actually supported
    auto supported = instanceInfo.validate(layers, instanceExtensions);

    std::stringstream instanceCreationPrompt;
    instanceCreationPrompt << "Try creating Vulkan instance with the following layers:";
    if (supported.layers.empty()) {
        instanceCreationPrompt << " <none>";
    } else {
        for (auto & l : supported.layers) { instanceCreationPrompt << " " << l; }
    }
    instanceCreationPrompt << std::endl << "Try creating Vulkan instance with the following extensions:";
    if (supported.instanceExtensions.empty()) {
        instanceCreationPrompt << " <none>";
    } else {
        for (auto & l : supported.instanceExtensions) { instanceCreationPrompt << " " << l; }
    }
    instanceCreationPrompt << std::endl;
    RAPID_VULKAN_LOG_INFO(instanceCreationPrompt.str().c_str());

    // create VK 1.1 instance
    // TODO: check against available version.
    auto appInfo = vk::ApplicationInfo().setApiVersion(_cp.apiVersion);
    auto ici     = vk::InstanceCreateInfo()
                   .setPNext(buildStructureChain(_cp.instanceCreateInfo.begin(), _cp.instanceCreateInfo.end()))
                   .setPApplicationInfo(&appInfo)
                   .setPEnabledLayerNames(supported.layers)
                   .setPEnabledExtensionNames(supported.instanceExtensions);
    _instance = vk::createInstance(ici);

    // Print instance information
    if (cp.printVkInfo) {
        auto message = instanceInfo.print(ici, Device::VERBOSE == cp.printVkInfo);
        RAPID_VULKAN_LOG_INFO(message.data());
    }

#if RAPID_VULKAN_ENABLE_LOADER
    // load all function pointers.
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);
#endif

    RAPID_VULKAN_LOG_INFO("Vulkan instance initialized.");
}

// ---------------------------------------------------------------------------------------------------------------------
//
Instance::~Instance() {
    if (_instance) _instance.destroy(), _instance = VK_NULL_HANDLE;
    RAPID_VULKAN_LOG_INFO("Vulkan instance destroyed.");
}

} // namespace RAPID_VULKAN_NAMESPACE
