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

// *********************************************************************************************************************
// Shader
// *********************************************************************************************************************

Shader Shader::EMPTY({});

Shader::Shader(const ConstructParameters & params): Root(params), _gi(params.gi) {
    if (params.spirv.empty()) return; // Constructing an empty shader module. Not an error.
    _handle = _gi.device.createShaderModule({{}, params.spirv.size() * sizeof(uint32_t), params.spirv.data()}, _gi.allocator);
    _entry  = params.entry;
}

Shader::~Shader() { _gi.safeDestroy(_handle); }

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

    CommandBufferImpl(const GlobalInfo & gi, uint32_t family, const std::string & name_, vk::CommandBufferLevel level): CommandBuffer({name_}), _gi(gi) {
        _pool = _gi.device.createCommandPool(vk::CommandPoolCreateInfo().setQueueFamilyIndex(family), _gi.allocator);

        vk::CommandBufferAllocateInfo info;
        info.commandPool        = _pool;
        info.level              = level;
        info.commandBufferCount = 1;
        _handle                 = _gi.device.allocateCommandBuffers(info)[0];
        _level                  = level;
        setVkObjectName(_gi.device, _handle, name());
        _handle.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    }

    virtual ~CommandBufferImpl() {
        _gi.safeDestroy(_handle, _pool);
        _gi.safeDestroy(_fence);
        _gi.safeDestroy(_semaphore);
        _gi.safeDestroy(_pool);
    }

    const GlobalInfo & gi() const { return _gi; }

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
        auto fence     = _gi.device.createFenceUnique({});
        auto semaphore = _gi.device.createSemaphoreUnique({});
        setVkObjectName(_gi.device, fence.get(), name());
        setVkObjectName(_gi.device, semaphore.get(), name());

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
            auto result = _gi.device.waitForFences({_fence}, true, UINT64_MAX);
            if (result != vk::Result::eSuccess) {
                RAPID_VULKAN_LOG_ERROR("[ERROR] Command buffer %s failed to wait for fence!", name().c_str());
            }
            _state = FINISHED;
        }
    }

    /// Only called on pending command buffers and unconditionally mark it as finished.
    void setFinished() {
        RAPID_VULKAN_ASSERT(_state == EXECUTING);
        _state = FINISHED;
    }

private:
    const GlobalInfo & _gi;
    vk::CommandPool    _pool; // one pool for each command buffer for simplicity for now.
    State              _state     = RECORDING;
    vk::Fence          _fence     = nullptr;
    vk::Semaphore      _semaphore = nullptr;
};

class CommandQueue::Impl {
public:
    Impl(CommandQueue & owner): _owner(owner) {}
    ~Impl() {}

    CommandBuffer * begin(const char * name, vk::CommandBufferLevel level) {
        auto lock = std::lock_guard {_mutex};
        auto p    = new CommandBufferImpl(_owner.gi(), _owner.family(), name, level);
        _all.insert(p);
        return p;
    }

    void submit(CommandBuffer * cb) {
        auto lock = std::lock_guard {_mutex};

        // check the incoming pointer
        auto p = promote(cb);
        if (!p) return;

        if (!p->submit(_owner.handle())) return;

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
    CommandQueue &   _owner;
    std::mutex       _mutex;
    CommandBufferSet _all;      /// All command buffers ever created. This is used to validate the incoming command buffer pointers.
    PendingList      _pendings; /// Command buffers that are currently being executed, sorted in order of submission.

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
            _owner._handle.waitIdle();
        } catch (vk::SystemError & error) { RAPID_VULKAN_LOG_ERROR(error.what()); }

        // then mark all command buffers as finished.
        finish(nullptr);
    }
};

CommandQueue::CommandQueue(const ConstructParameters & params): Root(params), _gi(params.gi), _family(params.family), _index(params.index) {
    _handle = _gi.device.getQueue(_family, _index);
    _impl   = new Impl(*this);
}
CommandQueue::~CommandQueue() { delete _impl; }
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

PipelineLayout::PipelineLayout(const GlobalInfo & gi, const PipelineReflection & refl): Root({refl.name}), _gi(gi) {
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
        _sets[i]        = gi.device.createDescriptorSetLayout(ci, gi.allocator);
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
    _handle                   = gi.device.createPipelineLayout(ci, gi.allocator);
    setVkObjectName(gi.device, _handle, refl.name);
}

PipelineLayout::~PipelineLayout() {
    for (auto & s : _sets) _gi.safeDestroy(s);
    _sets.clear();
    _gi.safeDestroy(_handle);
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
    auto & gi = shaders.front()->gi();
    _layout   = std::make_shared<PipelineLayout>(gi, _reflection);
}

Pipeline::~Pipeline() {
    if (_handle) {
        RAPID_VULKAN_ASSERT(_layout);
        _layout->gi().safeDestroy(_handle);
    }
}

// *********************************************************************************************************************
// Compute Pipeline
// *********************************************************************************************************************

ComputePipeline::ComputePipeline(const ConstructParameters & params): Pipeline(params.name, {&params.cs}), _gi(params.cs.gi()) {
    vk::ComputePipelineCreateInfo ci;
    ci.setStage({{}, vk::ShaderStageFlagBits::eCompute, params.cs.handle(), params.cs.entry().c_str()});
    ci.setLayout(_layout->handle());
    _handle = _gi.device.createComputePipeline(nullptr, ci, _gi.allocator).value;
}

auto ComputePipeline::createArguments() -> std::unique_ptr<PipelineArguments> { return {}; }

void ComputePipeline::bind(vk::CommandBuffer cb, const PipelineArguments &) { cb.bindPipeline(vk::PipelineBindPoint::eCompute, _handle); }

void ComputePipeline::dispatch(vk::CommandBuffer cb, const DispatchParameters & dp) {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, _handle);
    cb.dispatch(dp.width, dp.height, dp.depth);
}

} // namespace RAPID_VULKAN_NAMESPACE
