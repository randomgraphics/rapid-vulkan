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

#undef RAPID_VULKAN_IMPLEMENTATION
#include "rapid-vulkan.h"

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
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

#include "3rd-party/spriv-reflect/spirv_reflect.c"
#ifdef RVI_NEED_VMA_IMPL
#define VMA_IMPLEMENTATION
#include "3rd-party/vma-3.0.1/vk_mem_alloc.h"

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif // RVI_NEED_VMA_IMPL

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <mutex>
#include <variant>
#include <unordered_map>
#include <list>
#include <set>
#include <signal.h>

#if RAPID_VULKAN_ENABLE_LOADER
// implement the default dynamic dispatcher storage. Has to use this macro outside of any namespace.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;
#endif

#ifdef _WIN32
extern "C" __declspec(dllimport) void __stdcall DebugBreak();
#endif

namespace RAPID_VULKAN_NAMESPACE {

#if !RAPID_VULKAN_ENABLE_VMA
using VmaAllocation = int;
#endif

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

// ---------------------------------------------------------------------------------------------------------------------
//
void threadSafeWaitForDeviceIdle(vk::Device device) {
    static std::mutex mutex;
    auto              lock = std::lock_guard {mutex};
    device.waitIdle();
}

// *********************************************************************************************************************
// Command Buffer/Pool/Queue
// *********************************************************************************************************************

class CommandBuffer {
public:
    enum State {
        RECORDING,
        ENDED,
        EXECUTING,
        FINISHED,
    };

    std::list<CommandBuffer *>::iterator pending = {};

    CommandBuffer(const GlobalInfo * gi, uint32_t family, const std::string & name_, vk::CommandBufferLevel level): _gi(gi), _name(name_) {
        RVI_ASSERT(_gi);
        _pool = _gi->device.createCommandPool(vk::CommandPoolCreateInfo().setQueueFamilyIndex(family), _gi->allocator);

        vk::CommandBufferAllocateInfo info;
        info.commandPool        = _pool;
        info.level              = level;
        info.commandBufferCount = 1;
        _handle                 = _gi->device.allocateCommandBuffers(info)[0];
        _level                  = level;
        setVkObjectName(_gi->device, _handle, name_);
        _handle.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    }

    virtual ~CommandBuffer() {
        _gi->safeDestroy(_handle, _pool);
        _gi->safeDestroy(_semaphore);
        _gi->safeDestroy(_pool);
    }

    const GlobalInfo * gi() const { return _gi; }

    const std::string & name() const { return _name; }

    vk::CommandBuffer handle() const { return _handle; }

    State state() const { return _state; }

    bool submit(vk::Queue queue, const CommandQueue::SubmitParameters & sp) {
        // verify the command buffer state
        if (_state == RECORDING) {
            // end the command buffer
            _handle.end();
            _state = ENDED;
        } else if (_state != ENDED) {
            RVI_LOGE("[ERROR] Command buffer %s is not in RECORDING or ENDED state!", _name.c_str());
            return false;
        }

        // setup signal fence
        RVI_ASSERT(!_builtInSubmissionFence);
        auto fence = sp.signalFence;
        if (!fence) {
            _builtInSubmissionFence = _gi->device.createFenceUnique({});
            setVkObjectName(_gi->device, _builtInSubmissionFence.get(), _name);
            fence = _builtInSubmissionFence.get();
        }

        // setup signal semaphore
        RVI_ASSERT(!_semaphore);
        auto semaphore = _gi->device.createSemaphoreUnique({});
        setVkObjectName(_gi->device, semaphore.get(), _name);
        std::vector<vk::Semaphore> signalSemaphores(sp.signalSemaphores.begin(), sp.signalSemaphores.end());
        signalSemaphores.push_back(semaphore.get());

        // setup wait semaphore flag array
        std::vector<vk::PipelineStageFlags> flags(sp.waitSemaphores.size(), vk::PipelineStageFlagBits::eBottomOfPipe);

        // submit the command buffer
        vk::SubmitInfo si;
        si.setWaitSemaphores(sp.waitSemaphores);
        si.setSignalSemaphores(signalSemaphores);
        si.setCommandBufferCount(1);
        si.setPCommandBuffers(&_handle);
        si.setPWaitDstStageMask(flags.data());
        queue.submit({si}, fence);

        // done
        _state          = EXECUTING;
        _effectiveFence = fence; // store the fence handle (could be user provided or built-in)
        _semaphore      = semaphore.release();
        return true;
    }

    // wait for the command buffer to finish executing on GPU.
    void finish() {
        if (EXECUTING == _state) {
            RVI_ASSERT(_effectiveFence);
            auto result = _gi->device.waitForFences({_effectiveFence}, true, UINT64_MAX);
            if (result != vk::Result::eSuccess) { RVI_LOGE("[ERROR] Command buffer %s failed to wait for fence!", _name.c_str()); }
            _state = FINISHED;
        }
    }

    /// Only called on pending command buffers and unconditionally mark it as finished.
    /// Should only be called on a pending or already finished command buffer.
    void setFinished() {
        RVI_ASSERT(_state == EXECUTING || _state == FINISHED);
        _state = FINISHED;
    }

private:
    const GlobalInfo *     _gi;
    std::string            _name;
    vk::CommandPool        _pool; // one pool for each command buffer for simplicity for now.
    vk::CommandBuffer      _handle {};
    vk::CommandBufferLevel _level = vk::CommandBufferLevel::ePrimary;
    State                  _state = RECORDING;
    vk::UniqueFence        _builtInSubmissionFence;
    vk::Fence              _effectiveFence = nullptr;
    vk::Semaphore          _semaphore      = nullptr;
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

    vk::CommandBuffer begin(const char * name, vk::CommandBufferLevel level) {
        auto lock = std::lock_guard {_mutex};
        auto p    = std::make_unique<CommandBuffer>(_desc.gi, _desc.family, name, level);
        auto h    = p->handle();
        _all[h]   = std::move(p);
        return h;
    }

    void submit(const SubmitParameters & sp) {
        auto lock = std::lock_guard {_mutex};

        // check the incoming pointer
        auto p = promote(sp.commands);
        RVI_REQUIRE(p, "The submitted command buffer is not created by this queue!");

        RVI_REQUIRE(p->submit(_desc.handle, sp));

        // Done. add the command buffer to the pending list.
        p->pending = _pendings.insert(_pendings.end(), p);
    }

    void wait(vk::CommandBuffer cb) {
        auto lock = std::lock_guard {_mutex};
        if (!cb) {
            waitIdle();
            return;
        }
        auto p = promote(cb, true); // could be null if the command buffer is already finished.
        if (!p) return;
        finish(p);
    }

    void setName(const std::string & name) {
        auto lock = std::lock_guard {_mutex};
        setVkObjectName(_desc.gi->device, _desc.handle, name.c_str());
    }

private:
    typedef std::unordered_map<VkCommandBuffer, std::unique_ptr<CommandBuffer>> CommandBufferSet;
    typedef std::list<CommandBuffer *>                                          PendingList;

private:
    std::mutex       _mutex;
    CommandBufferSet _all;      /// All command buffers ever created. This is used to validate the incoming command buffer pointers.
    PendingList      _pendings; /// Command buffers that are currently being executed, sorted in order of submission.
    Desc             _desc;

private:
    CommandBuffer * promote(vk::CommandBuffer cb, bool expectedNull = false) const {
        if (!cb) return nullptr;
        auto it = _all.find(cb);
        if (it == _all.end()) {
            if (!expectedNull) RVI_LOGE("Invalid command buffer handle.");
            return nullptr;
        }
        return it->second.get();
    }

    void finish(CommandBuffer * p) {
        if (p) {
            p->finish();
            if (CommandBuffer::FINISHED != p->state()) {
                RVI_LOGE("Can't finish command buffer %s: buffer was not in EXECUTING or FINISHED state.", p->name().c_str());
                return;
            }
            RAPID_VULKAN_ASSERT(std::find(_pendings.begin(), _pendings.end(), p) != _pendings.end());
        }

        // Mark this command buffer and all command buffers submitted before it as finished.
        auto end = p ? p->pending : _pendings.end();
        if (p) end++;
        for (auto iter = _pendings.begin(); iter != end; ++iter) {
            auto cb = *iter;
            cb->setFinished();
            // TODO: move the command buffer to the finished list.
            // _finished.push_back(std::move(*iter));

            // Remove from all list, which will automatically delete the command buffer instance.
            _all.erase(_all.find(cb->handle()));
        }
        // Then remove them from the pending list.
        _pendings.erase(_pendings.begin(), end);
    }

    void waitIdle() {
        try {
            _desc.handle.waitIdle();
        } catch (vk::SystemError & error) { RVI_LOGE("%s", error.what()); }

        // then mark all command buffers as finished.
        finish(nullptr);
    }
};

CommandQueue::CommandQueue(const ConstructParameters & params): Root(params), _impl(new Impl(params)) { _impl->setName(name()); }
CommandQueue::~CommandQueue() {
    delete _impl;
    _impl = nullptr;
}
auto CommandQueue::desc() const -> const Desc & { return _impl->desc(); }
auto CommandQueue::begin(const char * purpose, vk::CommandBufferLevel level) -> vk::CommandBuffer { return _impl->begin(purpose, level); }
void CommandQueue::submit(const SubmitParameters & sp) { _impl->submit(sp); }
void CommandQueue::wait(vk::CommandBuffer cb) { _impl->wait(cb); }
void CommandQueue::onNameChanged(const std::string &) { _impl->setName(name()); }

// *********************************************************************************************************************
// Buffer
// *********************************************************************************************************************

/// TODO: Defer to VMA for memory allocations if it's enabled
static vk::DeviceMemory allocateDeviceMemory(const GlobalInfo & g, const vk::MemoryRequirements & memRequirements, vk::MemoryPropertyFlags memoryProperties,
                                             const vk::MemoryAllocateFlags allocFlags) {
    auto memProperties = g.physical.getMemoryProperties();

    uint32_t memoryIndex = uint32_t(-1);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (0 == (memRequirements.memoryTypeBits & (1 << i))) continue;
        auto propFlags = memProperties.memoryTypes[i].propertyFlags;
        if (memoryProperties != (propFlags & memoryProperties)) continue;
        // found the memory type we need
        memoryIndex = i;
        break;
    }
    if (memoryIndex > memProperties.memoryTypeCount) { RVI_THROW("Can't find a memory type that supports the required memory usage."); }

    auto ai  = vk::MemoryAllocateInfo(memRequirements.size, memoryIndex);
    auto afi = vk::MemoryAllocateFlagsInfo {allocFlags};
    if (allocFlags) ai.setPNext(&afi);

    return g.device.allocateMemory(ai);
}

class Buffer::Impl {
public:
    Impl(Buffer & owner, const ConstructParameters & cp): _owner(owner), _gi(cp.gi) {
        RVI_ASSERT(_gi);

        auto ci  = vk::BufferCreateInfo {};
        ci.size  = cp.size;
        ci.usage = cp.usage | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc;

#if RAPID_VULKAN_ENABLE_VMA
        if (_gi->vmaAllocator) {
            VmaAllocationCreateInfo aci {};
            aci.requiredFlags = (VkMemoryPropertyFlags) cp.memory;
            RVI_VK_REQUIRE(vmaCreateBuffer(_gi->vmaAllocator, (const VkBufferCreateInfo *) &ci, &aci, (VkBuffer *) &_handle, &_allocation, nullptr));
        } else
#endif
        {
            // create buffer
            _handle = _gi->device.createBuffer(ci, _gi->allocator);
            // allocate & bind memory
            auto requirements = _gi->device.getBufferMemoryRequirements(_handle);
            _memory           = allocateDeviceMemory(*_gi, requirements, cp.memory, cp.alloc);
            _gi->device.bindBufferMemory(_handle, _memory, 0);
        }

        // set buffer name.
        onNameChanged();

        // done
        _desc.size   = cp.size;
        _desc.usage  = cp.usage;
        _desc.memory = cp.memory;
        _desc.handle = _handle;
    }

    Impl(Buffer & owner, const ImportParameters &): _owner(owner) {}

    ~Impl() {
#if RAPID_VULKAN_ENABLE_VMA
        if (_allocation) {
            RVI_ASSERT(!_memory);
            vmaDestroyBuffer(_gi->vmaAllocator, _handle, _allocation);
        } else
#endif
        {
            if (_handle) _gi->safeDestroy(_handle);
            if (_memory) _gi->safeDestroy(_memory);
        }
    }

    auto desc() const -> const Desc & { return _desc; }

    void cmdCopy(const CopyParameters & params) {
        if (!params.cb) {
            RVI_LOGE("Can't copy buffer: command buffer is null.");
            return;
        }
        if (!params.dst) {
            RVI_LOGE("Can't copy buffer: destination buffer is null.");
            return;
        }
        if (0 == params.dstCapacity) {
            RVI_LOGE("Can't copy buffer: destination capacity is 0.");
            return;
        }

        // clamp the copy range.
        auto srcOffset = params.srcOffset;
        auto dstOffset = params.dstOffset;
        auto size      = params.size;
        clampRange2(srcOffset, dstOffset, size, _desc.size, params.dstCapacity);
        if (0 == size) return;

        RVI_ASSERT(srcOffset + size <= _desc.size);
        RVI_ASSERT(dstOffset + size <= params.dstCapacity);

        params.cb.copyBuffer(_desc.handle, params.dst, {{srcOffset, dstOffset, size}});
    }

    void setContent(const SetContentParameters & params) {
        // validate parameters
        auto dstOffset = params.offset;
        auto size      = params.size;
        auto srcOffset = clampRange(dstOffset, size, _desc.size);
        if (0 == size) return;
        if (!params.data) {
            RVI_LOGE("Can't set buffer content: data pointer is null.");
            return;
        }
        auto source = (const uint8_t *) params.data + srcOffset;

        // copy data into the staging buffer.
        auto staging = Buffer(ConstructParameters {{_owner.name()}, _gi, size}.setStaging());
        auto m       = Map<uint8_t>(staging);
        memcpy(m.data, source, size);
        m.unmap();

        // copy to the target buffer.
        auto queue = CommandQueue({{_owner.name()}, _gi, params.queueFamily, params.queueIndex});
        if (auto cb = queue.begin(_owner.name().c_str(), vk::CommandBufferLevel::ePrimary)) {
            staging.cmdCopy({cb, _owner.handle(), _desc.size, dstOffset, 0, size});
            queue.submit({cb});
            queue.wait(cb);
        }
    }

    auto readContent(const ReadParameters & params) -> std::vector<uint8_t> {
        // validate reading range.
        auto offset = params.offset;
        auto size   = params.size;
        clampRange(offset, size, _desc.size);
        if (0 == size) return {};

        // Always copy buffer content to another staging buffer. This is to ensure all pending
        // work items in the queue are finished before we read the buffer content.
        // TODO: change buffer barrier to make it a copy source.
        auto staging = Buffer(Buffer::ConstructParameters {{_owner.name()}, _gi, size}.setStaging());
        auto queue   = CommandQueue({{_owner.name()}, _gi, params.queueFamily, params.queueIndex});
        if (auto cb = queue.begin(_owner.name().c_str(), vk::CommandBufferLevel::ePrimary)) {
            cmdCopy({cb, staging.handle(), size, 0, offset});
            queue.submit({cb});
            queue.wait(cb);
        } else {
            return {};
        }

        // read data from the staging buffer
        auto m = Map<uint8_t>(staging);
        RVI_ASSERT(0 == m.offset);
        RVI_ASSERT(m.length == size);
        return std::vector<uint8_t>(m.data, m.data + m.length);
    }

    auto map(const MapParameters & params) -> MappedResult {
        auto lock = std::lock_guard {_mutex};
        if (_mapped) {
            RVI_LOGE("buffer %s is already mapped.", _owner.name().c_str());
            return {};
        }
        if (imported()) {
            RVI_LOGE("Can't map imported buffer %s, since we don't have it memory handle.", _owner.name().c_str());
            return {};
        }
        if (!_desc.mappable()) {
            RVI_LOGE("buffer %s is not mappable.", _owner.name().c_str());
            return {};
        }
        auto o = params.offset;
        auto s = params.size;
        clampRange(o, s, _desc.size);
        if (0 == s) {
            RVI_LOGE("mapped range is invalid or empty.");
            return {};
        }
        // TODO: VMA
        // if (allocation) {
        //     RVI_REQUIRE(global->vmaAllocator);
        //     RVI_VK_REQUIRE(vmaMapMemory(global->vmaAllocator, allocation, (void **) &dst));
        //     dst += offsetInUnitOfT;
        // } else {
        auto p = _gi->device.mapMemory(_memory, o, s);
        if (!p) {
            RVI_LOGE("Failed to map buffer %s.", _owner.name().c_str());
            return {};
        }
        _mapped = true;
        return {(uint8_t *) p, o, s};
    }

    void unmap() {
        auto lock = std::lock_guard {_mutex};
        if (_mapped) {
            _gi->device.unmapMemory(_memory);
            _mapped = false;
        }
    }

    void onNameChanged() {
        const auto & name = _owner.name();
        if (_handle) setVkObjectName(_gi->device, _handle, name);
        if (_memory) setVkObjectName(_gi->device, _memory, name);
#if RAPID_VULKAN_ENABLE_VMA
        if (_allocation) vmaSetAllocationName(_gi->vmaAllocator, _allocation, name.c_str());
#endif
    }

private:
    Buffer &           _owner;
    const GlobalInfo * _gi;
    Desc               _desc;
    vk::Buffer         _handle {}; // this is the handle that we created. when importing buffer handle, this one is left empty.
    vk::DeviceMemory   _memory {}; // this is the memory that we allocated. when importing buffer handle, this one is left empty.
    VmaAllocation      _allocation {};
    bool               _mapped {false};
    std::mutex         _mutex;

private:
    bool imported() const { return _desc.handle && !_handle; }
};

Buffer::Buffer(const ConstructParameters & cp): Root(cp) { _impl = new Impl(*this, cp); }
Buffer::Buffer(const ImportParameters & cp): Root(cp) { _impl = new Impl(*this, cp); }
Buffer::~Buffer() {
    delete _impl;
    _impl = nullptr;
}
auto Buffer::desc() const -> const Desc & { return _impl->desc(); }
void Buffer::cmdCopy(const CopyParameters & p) { return _impl->cmdCopy(p); }
auto Buffer::setContent(const SetContentParameters & p) -> Buffer & {
    _impl->setContent(p);
    return *this;
}
auto Buffer::readContent(const ReadParameters & p) -> std::vector<uint8_t> { return _impl->readContent(p); }
auto Buffer::map(const MapParameters & p) -> MappedResult { return _impl->map(p); }
void Buffer::unmap() { return _impl->unmap(); }
void Buffer::onNameChanged(const std::string &) { _impl->onNameChanged(); }

// *********************************************************************************************************************
// Image
// *********************************************************************************************************************

struct VkFormatDesc {
    uint32_t sizeBytes;
    uint32_t blockW;
    uint32_t blockH;

    static VkFormatDesc get(vk::Format format) {
        static const VkFormatDesc table[] = {
            {0, 0, 0},                       // VK_FORMAT_UNDEFINED = 0,
            {(4 + 4) / 8, 1, 1},             // VK_FORMAT_R4G4_UNORM_PACK8 = 1,
            {(4 + 4 + 4 + 4) / 8, 1, 1},     // VK_FORMAT_R4G4B4A4_UNORM_PACK16 = 2,
            {(4 + 4 + 4 + 4) / 8, 1, 1},     // VK_FORMAT_B4G4R4A4_UNORM_PACK16 = 3,
            {(5 + 6 + 5) / 8, 1, 1},         // VK_FORMAT_R5G6B5_UNORM_PACK16 = 4,
            {(5 + 6 + 5) / 8, 1, 1},         // VK_FORMAT_B5G6R5_UNORM_PACK16 = 5,
            {(5 + 5 + 5 + 1) / 8, 1, 1},     // VK_FORMAT_R5G5B5A1_UNORM_PACK16 = 6,
            {(5 + 5 + 5 + 1) / 8, 1, 1},     // VK_FORMAT_B5G5R5A1_UNORM_PACK16 = 7,
            {(1 + 5 + 5 + 5) / 8, 1, 1},     // VK_FORMAT_A1R5G5B5_UNORM_PACK16 = 8,
            {(8) / 8, 1, 1},                 // VK_FORMAT_R8_UNORM = 9,
            {(8) / 8, 1, 1},                 // VK_FORMAT_R8_SNORM = 10,
            {(8) / 8, 1, 1},                 // VK_FORMAT_R8_USCALED = 11,
            {(8) / 8, 1, 1},                 // VK_FORMAT_R8_SSCALED = 12,
            {(8) / 8, 1, 1},                 // VK_FORMAT_R8_UINT = 13,
            {(8) / 8, 1, 1},                 // VK_FORMAT_R8_SINT = 14,
            {(8) / 8, 1, 1},                 // VK_FORMAT_R8_SRGB = 15,
            {(8 + 8) / 8, 1, 1},             // VK_FORMAT_R8G8_UNORM = 16,
            {(8 + 8) / 8, 1, 1},             // VK_FORMAT_R8G8_SNORM = 17,
            {(8 + 8) / 8, 1, 1},             // VK_FORMAT_R8G8_USCALED = 18,
            {(8 + 8) / 8, 1, 1},             // VK_FORMAT_R8G8_SSCALED = 19,
            {(8 + 8) / 8, 1, 1},             // VK_FORMAT_R8G8_UINT = 20,
            {(8 + 8) / 8, 1, 1},             // VK_FORMAT_R8G8_SINT = 21,
            {(8 + 8) / 8, 1, 1},             // VK_FORMAT_R8G8_SRGB = 22,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_R8G8B8_UNORM = 23,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_R8G8B8_SNORM = 24,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_R8G8B8_USCALED = 25,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_R8G8B8_SSCALED = 26,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_R8G8B8_UINT = 27,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_R8G8B8_SINT = 28,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_R8G8B8_SRGB = 29,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_B8G8R8_UNORM = 30,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_B8G8R8_SNORM = 31,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_B8G8R8_USCALED = 32,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_B8G8R8_SSCALED = 33,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_B8G8R8_UINT = 34,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_B8G8R8_SINT = 35,
            {(8 + 8 + 8) / 8, 1, 1},         // VK_FORMAT_B8G8R8_SRGB = 36,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_R8G8B8A8_UNORM = 37,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_R8G8B8A8_SNORM = 38,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_R8G8B8A8_USCALED = 39,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_R8G8B8A8_SSCALED = 40,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_R8G8B8A8_UINT = 41,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_R8G8B8A8_SINT = 42,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_R8G8B8A8_SRGB = 43,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_B8G8R8A8_UNORM = 44,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_B8G8R8A8_SNORM = 45,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_B8G8R8A8_USCALED = 46,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_B8G8R8A8_SSCALED = 47,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_B8G8R8A8_UINT = 48,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_B8G8R8A8_SINT = 49,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_B8G8R8A8_SRGB = 50,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_A8B8G8R8_UNORM_PACK32 = 51,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_A8B8G8R8_SNORM_PACK32 = 52,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_A8B8G8R8_USCALED_PACK32 = 53,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_A8B8G8R8_SSCALED_PACK32 = 54,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_A8B8G8R8_UINT_PACK32 = 55,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_A8B8G8R8_SINT_PACK32 = 56,
            {(8 + 8 + 8 + 8) / 8, 1, 1},     // VK_FORMAT_A8B8G8R8_SRGB_PACK32 = 57,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2R10G10B10_UNORM_PACK32 = 58,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2R10G10B10_SNORM_PACK32 = 59,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2R10G10B10_USCALED_PACK32 = 60,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2R10G10B10_SSCALED_PACK32 = 61,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2R10G10B10_UINT_PACK32 = 62,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2R10G10B10_SINT_PACK32 = 63,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2B10G10R10_UNORM_PACK32 = 64,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2B10G10R10_SNORM_PACK32 = 65,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2B10G10R10_USCALED_PACK32 = 66,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2B10G10R10_SSCALED_PACK32 = 67,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2B10G10R10_UINT_PACK32 = 68,
            {(2 + 10 + 10 + 10) / 8, 1, 1},  // VK_FORMAT_A2B10G10R10_SINT_PACK32 = 69,
            {(16) / 8, 1, 1},                // VK_FORMAT_R16_UNORM = 70,
            {(16) / 8, 1, 1},                // VK_FORMAT_R16_SNORM = 71,
            {(16) / 8, 1, 1},                // VK_FORMAT_R16_USCALED = 72,
            {(16) / 8, 1, 1},                // VK_FORMAT_R16_SSCALED = 73,
            {(16) / 8, 1, 1},                // VK_FORMAT_R16_UINT = 74,
            {(16) / 8, 1, 1},                // VK_FORMAT_R16_SINT = 75,
            {(16) / 8, 1, 1},                // VK_FORMAT_R16_SFLOAT = 76,
            {(16 + 16) / 8, 1, 1},           // VK_FORMAT_R16G16_UNORM = 77,
            {(16 + 16) / 8, 1, 1},           // VK_FORMAT_R16G16_SNORM = 78,
            {(16 + 16) / 8, 1, 1},           // VK_FORMAT_R16G16_USCALED = 79,
            {(16 + 16) / 8, 1, 1},           // VK_FORMAT_R16G16_SSCALED = 80,
            {(16 + 16) / 8, 1, 1},           // VK_FORMAT_R16G16_UINT = 81,
            {(16 + 16) / 8, 1, 1},           // VK_FORMAT_R16G16_SINT = 82,
            {(16 + 16) / 8, 1, 1},           // VK_FORMAT_R16G16_SFLOAT = 83,
            {(16 + 16 + 16) / 8, 1, 1},      // VK_FORMAT_R16G16B16_UNORM = 84,
            {(16 + 16 + 16) / 8, 1, 1},      // VK_FORMAT_R16G16B16_SNORM = 85,
            {(16 + 16 + 16) / 8, 1, 1},      // VK_FORMAT_R16G16B16_USCALED = 86,
            {(16 + 16 + 16) / 8, 1, 1},      // VK_FORMAT_R16G16B16_SSCALED = 87,
            {(16 + 16 + 16) / 8, 1, 1},      // VK_FORMAT_R16G16B16_UINT = 88,
            {(16 + 16 + 16) / 8, 1, 1},      // VK_FORMAT_R16G16B16_SINT = 89,
            {(16 + 16 + 16) / 8, 1, 1},      // VK_FORMAT_R16G16B16_SFLOAT = 90,
            {(16 + 16 + 16 + 16) / 8, 1, 1}, // VK_FORMAT_R16G16B16A16_UNORM = 91,
            {(16 + 16 + 16 + 16) / 8, 1, 1}, // VK_FORMAT_R16G16B16A16_SNORM = 92,
            {(16 + 16 + 16 + 16) / 8, 1, 1}, // VK_FORMAT_R16G16B16A16_USCALED = 93,
            {(16 + 16 + 16 + 16) / 8, 1, 1}, // VK_FORMAT_R16G16B16A16_SSCALED = 94,
            {(16 + 16 + 16 + 16) / 8, 1, 1}, // VK_FORMAT_R16G16B16A16_UINT = 95,
            {(16 + 16 + 16 + 16) / 8, 1, 1}, // VK_FORMAT_R16G16B16A16_SINT = 96,
            {(16 + 16 + 16 + 16) / 8, 1, 1}, // VK_FORMAT_R16G16B16A16_SFLOAT = 97,
            {(32) / 8, 1, 1},                // VK_FORMAT_R32_UINT = 98,
            {(32) / 8, 1, 1},                // VK_FORMAT_R32_SINT = 99,
            {(32) / 8, 1, 1},                // VK_FORMAT_R32_SFLOAT = 100,
            {(32 + 32) / 8, 1, 1},           // VK_FORMAT_R32G32_UINT = 101,
            {(32 + 32) / 8, 1, 1},           // VK_FORMAT_R32G32_SINT = 102,
            {(32 + 32) / 8, 1, 1},           // VK_FORMAT_R32G32_SFLOAT = 103,
            {(32 + 32 + 32) / 8, 1, 1},      // VK_FORMAT_R32G32B32_UINT = 104,
            {(32 + 32 + 32) / 8, 1, 1},      // VK_FORMAT_R32G32B32_SINT = 105,
            {(32 + 32 + 32) / 8, 1, 1},      // VK_FORMAT_R32G32B32_SFLOAT = 106,
            {(32 + 32 + 32 + 32) / 8, 1, 1}, // VK_FORMAT_R32G32B32A32_UINT = 107,
            {(32 + 32 + 32 + 32) / 8, 1, 1}, // VK_FORMAT_R32G32B32A32_SINT = 108,
            {(32 + 32 + 32 + 32) / 8, 1, 1}, // VK_FORMAT_R32G32B32A32_SFLOAT = 109,
            {(64) / 8, 1, 1},                // VK_FORMAT_R64_UINT = 110,
            {(64) / 8, 1, 1},                // VK_FORMAT_R64_SINT = 111,
            {(64) / 8, 1, 1},                // VK_FORMAT_R64_SFLOAT = 112,
            {(64 + 64) / 8, 1, 1},           // VK_FORMAT_R64G64_UINT = 113,
            {(64 + 64) / 8, 1, 1},           // VK_FORMAT_R64G64_SINT = 114,
            {(64 + 64) / 8, 1, 1},           // VK_FORMAT_R64G64_SFLOAT = 115,
            {(64 + 64 + 64) / 8, 1, 1},      // VK_FORMAT_R64G64B64_UINT = 116,
            {(64 + 64 + 64) / 8, 1, 1},      // VK_FORMAT_R64G64B64_SINT = 117,
            {(64 + 64 + 64) / 8, 1, 1},      // VK_FORMAT_R64G64B64_SFLOAT = 118,
            {(64 + 64 + 64 + 64) / 8, 1, 1}, // VK_FORMAT_R64G64B64A64_UINT = 119,
            {(64 + 64 + 64 + 64) / 8, 1, 1}, // VK_FORMAT_R64G64B64A64_SINT = 120,
            {(64 + 64 + 64 + 64) / 8, 1, 1}, // VK_FORMAT_R64G64B64A64_SFLOAT = 121,
            {(10 + 11 + 11) / 8, 1, 1},      // VK_FORMAT_B10G11R11_UFLOAT_PACK32 = 122,
            {(5 + 9 + 9 + 9) / 8, 1, 1},     // VK_FORMAT_E5B9G9R9_UFLOAT_PACK32 = 123,
            {(16) / 8, 1, 1},                // VK_FORMAT_D16_UNORM = 124,
            {(8) / 8, 1, 1},                 // VK_FORMAT_X8_D24_UNORM_PACK32 = 125,
            {(32) / 8, 1, 1},                // VK_FORMAT_D32_SFLOAT = 126,
            {(8) / 8, 1, 1},                 // VK_FORMAT_S8_UINT = 127,
            {(16) / 8, 1, 1},                // VK_FORMAT_D16_UNORM_S8_UINT = 128,
            {(24) / 8, 1, 1},                // VK_FORMAT_D24_UNORM_S8_UINT = 129,
            {(32) / 8, 1, 1},                // VK_FORMAT_D32_SFLOAT_S8_UINT = 130,
            {0, 4, 4},                       // VK_FORMAT_BC1_RGB_UNORM_BLOCK = 131,
            {0, 4, 4},                       // VK_FORMAT_BC1_RGB_SRGB_BLOCK = 132,
            {0, 4, 4},                       // VK_FORMAT_BC1_RGBA_UNORM_BLOCK = 133,
            {0, 4, 4},                       // VK_FORMAT_BC1_RGBA_SRGB_BLOCK = 134,
            {0, 4, 4},                       // VK_FORMAT_BC2_UNORM_BLOCK = 135,
            {0, 4, 4},                       // VK_FORMAT_BC2_SRGB_BLOCK = 136,
            {0, 4, 4},                       // VK_FORMAT_BC3_UNORM_BLOCK = 137,
            {0, 4, 4},                       // VK_FORMAT_BC3_SRGB_BLOCK = 138,
            {0, 4, 4},                       // VK_FORMAT_BC4_UNORM_BLOCK = 139,
            {0, 4, 4},                       // VK_FORMAT_BC4_SNORM_BLOCK = 140,
            {0, 4, 4},                       // VK_FORMAT_BC5_UNORM_BLOCK = 141,
            {0, 4, 4},                       // VK_FORMAT_BC5_SNORM_BLOCK = 142,
            {0, 4, 4},                       // VK_FORMAT_BC6H_UFLOAT_BLOCK = 143,
            {0, 4, 4},                       // VK_FORMAT_BC6H_SFLOAT_BLOCK = 144,
            {0, 4, 4},                       // VK_FORMAT_BC7_UNORM_BLOCK = 145,
            {0, 4, 4},                       // VK_FORMAT_BC7_SRGB_BLOCK = 146,
            {0, 4, 4},                       // VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147,
            {0, 4, 4},                       // VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK = 148,
            {0, 4, 4},                       // VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK = 149,
            {0, 4, 4},                       // VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK = 150,
            {0, 4, 4},                       // VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK = 151,
            {0, 4, 4},                       // VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK = 152,
            {0, 4, 4},                       // VK_FORMAT_EAC_R11_UNORM_BLOCK = 153,
            {0, 4, 4},                       // VK_FORMAT_EAC_R11_SNORM_BLOCK = 154,
            {0, 4, 4},                       // VK_FORMAT_EAC_R11G11_UNORM_BLOCK = 155,
            {0, 4, 4},                       // VK_FORMAT_EAC_R11G11_SNORM_BLOCK = 156,
            {0, 4, 4},                       // VK_FORMAT_ASTC_4x4_UNORM_BLOCK = 157,
            {0, 4, 4},                       // VK_FORMAT_ASTC_4x4_SRGB_BLOCK = 158,
            {0, 5, 4},                       // VK_FORMAT_ASTC_5x4_UNORM_BLOCK = 159,
            {0, 5, 4},                       // VK_FORMAT_ASTC_5x4_SRGB_BLOCK = 160,
            {0, 5, 5},                       // VK_FORMAT_ASTC_5x5_UNORM_BLOCK = 161,
            {0, 5, 5},                       // VK_FORMAT_ASTC_5x5_SRGB_BLOCK = 162,
            {0, 6, 5},                       // VK_FORMAT_ASTC_6x5_UNORM_BLOCK = 163,
            {0, 6, 5},                       // VK_FORMAT_ASTC_6x5_SRGB_BLOCK = 164,
            {0, 6, 6},                       // VK_FORMAT_ASTC_6x6_UNORM_BLOCK = 165,
            {0, 6, 6},                       // VK_FORMAT_ASTC_6x6_SRGB_BLOCK = 166,
            {0, 8, 5},                       // VK_FORMAT_ASTC_8x5_UNORM_BLOCK = 167,
            {0, 8, 5},                       // VK_FORMAT_ASTC_8x5_SRGB_BLOCK = 168,
            {0, 8, 6},                       // VK_FORMAT_ASTC_8x6_UNORM_BLOCK = 169,
            {0, 8, 6},                       // VK_FORMAT_ASTC_8x6_SRGB_BLOCK = 170,
            {0, 8, 8},                       // VK_FORMAT_ASTC_8x8_UNORM_BLOCK = 171,
            {0, 8, 8},                       // VK_FORMAT_ASTC_8x8_SRGB_BLOCK = 172,
            {0, 10, 5},                      // VK_FORMAT_ASTC_10x5_UNORM_BLOCK = 173,
            {0, 10, 5},                      // VK_FORMAT_ASTC_10x5_SRGB_BLOCK = 174,
            {0, 10, 6},                      // VK_FORMAT_ASTC_10x6_UNORM_BLOCK = 175,
            {0, 10, 6},                      // VK_FORMAT_ASTC_10x6_SRGB_BLOCK = 176,
            {0, 10, 8},                      // VK_FORMAT_ASTC_10x8_UNORM_BLOCK = 177,
            {0, 10, 8},                      // VK_FORMAT_ASTC_10x8_SRGB_BLOCK = 178,
            {0, 10, 10},                     // VK_FORMAT_ASTC_10x10_UNORM_BLOCK = 179,
            {0, 10, 10},                     // VK_FORMAT_ASTC_10x10_SRGB_BLOCK = 180,
            {0, 12, 10},                     // VK_FORMAT_ASTC_12x10_UNORM_BLOCK = 181,
            {0, 12, 10},                     // VK_FORMAT_ASTC_12x10_SRGB_BLOCK = 182,
            {0, 12, 12},                     // VK_FORMAT_ASTC_12x12_UNORM_BLOCK = 183,
            {0, 12, 12},                     // VK_FORMAT_ASTC_12x12_SRGB_BLOCK = 184,
        };
        static_assert(std::size(table) == (size_t) vk::Format::eAstc12x12SrgbBlock + 1, "table size mismatch");
        static const std::unordered_map<VkFormat, VkFormatDesc> extra = {
            {VK_FORMAT_G8B8G8R8_422_UNORM, {4, 1, 1}},
            {VK_FORMAT_B8G8R8G8_422_UNORM, {4, 1, 1}},
            {VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM, {0, 1, 1}},
            {VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, {0, 1, 1}},
            {VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM, {0, 1, 1}},
            {VK_FORMAT_G8_B8R8_2PLANE_422_UNORM, {0, 1, 1}},
            {VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM, {0, 1, 1}},
            {VK_FORMAT_R10X6_UNORM_PACK16, {2, 1, 1}},
            {VK_FORMAT_R10X6G10X6_UNORM_2PACK16, {4, 2, 1}},
            {VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16, {8, 1, 1}},
            {VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16, {8, 1, 1}},
            {VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16, {8, 1, 1}},
            {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_R12X4_UNORM_PACK16, {2, 1, 1}},
            {VK_FORMAT_R12X4G12X4_UNORM_2PACK16, {0, 1, 1}},
            {VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16, {0, 1, 1}},
            {VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16, {0, 1, 1}},
            {VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16, {0, 1, 1}},
            {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G16B16G16R16_422_UNORM, {0, 1, 1}},
            {VK_FORMAT_B16G16R16G16_422_UNORM, {0, 1, 1}},
            {VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM, {0, 1, 1}},
            {VK_FORMAT_G16_B16R16_2PLANE_420_UNORM, {0, 1, 1}},
            {VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM, {0, 1, 1}},
            {VK_FORMAT_G16_B16R16_2PLANE_422_UNORM, {0, 1, 1}},
            {VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM, {0, 1, 1}},
#if VK_HEADER_VERSION >= 239
            {VK_FORMAT_G8_B8R8_2PLANE_444_UNORM, {0, 1, 1}},
            {VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16, {0, 1, 1}},
            {VK_FORMAT_G16_B16R16_2PLANE_444_UNORM, {0, 1, 1}},
            {VK_FORMAT_A4R4G4B4_UNORM_PACK16, {2, 1, 1}},
            {VK_FORMAT_A4B4G4R4_UNORM_PACK16, {2, 1, 1}},
            {VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK, {0, 4, 4}},
            {VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK, {0, 5, 4}},
            {VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK, {0, 5, 5}},
            {VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK, {0, 6, 5}},
            {VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK, {0, 6, 6}},
            {VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK, {0, 8, 5}},
            {VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK, {0, 8, 6}},
            {VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK, {0, 8, 8}},
            {VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK, {0, 10, 5}},
            {VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK, {0, 10, 6}},
            {VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK, {0, 10, 8}},
            {VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK, {0, 10, 10}},
            {VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK, {0, 12, 10}},
            {VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK, {0, 12, 12}},
            {VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG, {0, 1, 1}},
            {VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG, {0, 1, 1}},
            {VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG, {0, 1, 1}},
            {VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG, {0, 1, 1}},
            {VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG, {0, 1, 1}},
            {VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG, {0, 1, 1}},
            {VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG, {0, 1, 1}},
            {VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG, {0, 1, 1}},
            {VK_FORMAT_R16G16_S10_5_NV, {0, 1, 1}},
#endif
        };
        auto i = (size_t) format;
        if (i < std::size(table)) {
            return table[i];
        } else {
            auto iter = extra.find((VkFormat) format);
            if (iter != extra.end()) return iter->second;
            RVI_LOGE("Unknown format: %d", (int) format);
            return {0, 0, 0};
        }
    }
};

class Image::Impl {
public:
    Impl(Image & o, const ConstructParameters & cp): _owner(o), _gi(cp.gi), _cp(cp) {
        RVI_REQUIRE(cp.gi);

        // check image format.
        auto fd = VkFormatDesc::get(_cp.info.format);
        if (0 == fd.sizeBytes || 0 == fd.blockW || 0 == fd.blockH) { RVI_THROW("unsupported image format %d", (int) _cp.info.format); }

        // update image usage to include transfer source and destination.
        _cp.info.usage |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;

        // update mipmap level count
        uint32_t maxLevels = (uint32_t) std::floor(std::log2((double) std::max(_cp.info.extent.width, _cp.info.extent.height))) + 1;
        if (_cp.info.mipLevels > maxLevels) {
            RVI_LOGW("mipmap level count %u is too large, clamped to %u", _cp.info.mipLevels, maxLevels);
            _cp.info.mipLevels = maxLevels;
        } else if (0 == _cp.info.mipLevels) {
            _cp.info.mipLevels = maxLevels;
        }

        // create image handle and memory
#if RAPID_VULKAN_ENABLE_VMA
        if (_gi->vmaAllocator) {
            VmaAllocationCreateInfo aci {};
            aci.requiredFlags = (VkMemoryPropertyFlags) _cp.memory;
            RVI_VK_REQUIRE(vmaCreateImage(_gi->vmaAllocator, (const VkImageCreateInfo *) &_cp.info, &aci, (VkImage *) &_handle, &_allocation, nullptr));
        } else
#endif
        {
            _handle = _gi->device.createImage(_cp.info, _gi->allocator);
            _memory = allocateDeviceMemory(*_gi, _gi->device.getImageMemoryRequirements(_handle), _cp.memory, _cp.alloc);
            _gi->device.bindImageMemory(_handle, _memory, 0);
        }

        onNameChanged();

        // store image description
        _desc.handle      = _handle;
        _desc.type        = _cp.info.imageType;
        _desc.format      = _cp.info.format;
        _desc.extent      = _cp.info.extent;
        _desc.mipLevels   = _cp.info.mipLevels;
        _desc.arrayLayers = _cp.info.arrayLayers;
        _desc.samples     = _cp.info.samples;

        // // create a default image view that covers the whole image
        // auto aspect          = determineImageAspect(ci.aspect, ci.format);
        // auto vci             = VkImageViewCreateInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        // vci.image            = image;
        // vci.viewType         = ci.isCube() ? VK_IMAGE_VIEW_TYPE_CUBE : ci.arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        // vci.format           = ci.format;
        // vci.components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        // vci.subresourceRange = {aspect, 0, ci.mipLevels, 0, ci.arrayLayers};
        // RVI_VK_REQUIRE(vkCreateImageView(g.device, &vci, g.allocator, &view));
        // setVkObjectName(g.device, view, name);
    }

    ~Impl() {
        for (auto & kv : _views) {
            auto view = kv.second;
            if (view) _gi->safeDestroy(view);
        }
        _views.clear();
#if RAPID_VULKAN_ENABLE_VMA
        if (_allocation) {
            vmaDestroyImage(_gi->vmaAllocator, _handle, _allocation);
        } else
#endif
        {
            _gi->safeDestroy(_handle);
            _gi->safeDestroy(_memory);
        }
    }

    const Desc & desc() const { return _desc; }

    vk::ImageView getView(GetViewParameters p) const {
        if (p.format == vk::Format::eUndefined) p.format = _desc.format;
        p.range.aspectMask = determineImageAspect(p.format, p.range.aspectMask);
        p.type             = determineViewType(p.type, p.range);
        if ((int) p.type < 0) return {};
        auto & view = _views[p];
        if (!view) view = _gi->device.createImageView(vk::ImageViewCreateInfo({}, _handle, p.type, p.format).setSubresourceRange(p.range), _gi->allocator);
        return view;
    }

    void setContent(const SetContentParameters & params) {
        // make sure area is aligned to block size
        auto formatDesc = VkFormatDesc::get(_desc.format);
        if ((params.area.x % formatDesc.blockW) != 0 || (params.area.y % formatDesc.blockH) != 0 || (params.area.w % formatDesc.blockW) != 0 ||
            (params.area.h % formatDesc.blockH) != 0) {
            RVI_LOGE("Image::setContent: area is not aligned to block size");
            return;
        }

        // validate row pitch
        const auto mipExtent = getMipExtent(params.mipLevel);
        auto       width     = params.area.w;
        if (uint32_t(-1) == width) width = mipExtent.width;
        auto rowPitch = params.pitch;
        if (0 == rowPitch) { rowPitch = width * formatDesc.sizeBytes; }
        if (rowPitch < width * formatDesc.sizeBytes) {
            RVI_LOGE("Image::setContent: row pitch is too small");
            return;
        }

        // TODO: validate mip level and array layer.

        // adjust area to fit image size
        auto area = clampRect3D(params.area, mipExtent);
        if (area.w == 0 || area.h == 0 || area.d == 0) return;

        // adjust pixel array pointer and size based on the clamped area.
        // (TODO: revisit this math. could be wrong)
        auto offset   = (area.x - params.area.x) * formatDesc.sizeBytes + (area.y - params.area.y) * rowPitch + (area.z - params.area.z) * rowPitch * area.h;
        auto pixels   = (const uint8_t *) params.pixels + offset;
        auto dataSize = rowPitch * area.h * area.d - rowPitch + area.w * formatDesc.sizeBytes - offset;

        // Copy texture data into staging buffer
        auto staging = Buffer(Buffer::ConstructParameters {{_owner.name()}, _gi, dataSize}.setStaging());
        auto mapped  = staging.map({});
        RVI_ASSERT(mapped.data);
        RVI_ASSERT(mapped.size == dataSize);
        memcpy(mapped.data, pixels, dataSize);
        staging.unmap();

        // determine subresource aspect
        auto aspect = determineImageAspect(_desc.format);

        // Setup buffer copy regions for the subresource
        auto copyRegion = vk::BufferImageCopy()
                              .setImageSubresource({aspect, params.mipLevel, params.arrayLayer, 1})
                              .setImageOffset({(int) area.x, (int) area.y, (int) area.z})
                              .setImageExtent({area.w, area.h, area.d});

        // copy image content from staging buffer to image
        auto q = CommandQueue({{_owner.name()}, _gi, params.queueFamily, params.queueIndex});
        auto c = q.begin(_owner.name().data());
        if (c) {
            auto r = vk::ImageSubresourceRange(aspect, params.mipLevel, 1, params.arrayLayer, 1);
            Barrier {}
                .s(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer)
                .i(_handle, vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eTransferDstOptimal, r)
                .cmdWrite(c);
            c.copyBufferToImage(staging, _handle, vk::ImageLayout::eTransferDstOptimal, {copyRegion});
            q.submit({c});
            q.wait(c);
        }
    }

    Content readContent(const ReadContentParameters & params) {
        // uint32_t mipLevel   = params.mipLevel;
        // uint32_t levelCount = params.layerCount;
        // uint32_t arrayLayer = params.arrayLayer;
        // uint32_t layerCount = params.layerCount;
        // clampRange(mipLevel, levelCount, _desc.mipLevels);
        // clampRange(arrayLayer, layerCount, _desc.arrayLayers);
        // if (0 == levelCount || 0 == layerCount) return {};

        auto formatDesc = VkFormatDesc::get(_desc.format);
        auto aspect     = determineImageAspect(_desc.format);
        auto mipExtents = buildMipExtentArray();

        Content                          content;
        std::vector<vk::BufferImageCopy> copyRegions;
        vk::DeviceSize                   dataSize = 0;
        for (uint32_t m = 0; m < mipExtents.size(); ++m) {
            auto extent   = mipExtents[m];
            auto rowPitch = extent.width * formatDesc.sizeBytes;
            auto mipSize  = rowPitch * extent.height * extent.depth;
            for (uint32_t a = 0; a < _desc.arrayLayers; ++a) {
                copyRegions.push_back(vk::BufferImageCopy()
                                          .setBufferOffset(dataSize)
                                          .setBufferRowLength(extent.width)
                                          .setBufferImageHeight(extent.height)
                                          .setImageSubresource({aspect, m, a, 1})
                                          .setImageOffset({0, 0, 0})
                                          .setImageExtent(extent));
                content.subresources.push_back({m, a, extent, rowPitch, dataSize});
                dataSize += mipSize;
            }
        }

        // Allocate staging buffer
        auto staging = Buffer(Buffer::ConstructParameters {{_owner.name()}, _gi, dataSize}.setStaging());

        // Copy image content into the staging buffer
        auto q = CommandQueue({{_owner.name()}, _gi, params.queueFamily, params.queueIndex});
        auto c = q.begin(_owner.name().data());
        if (c) {
            auto r = vk::ImageSubresourceRange(aspect, 0, _desc.mipLevels, 0, _desc.arrayLayers);
            Barrier {}
                .s(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer)
                .i(_handle, vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eTransferSrcOptimal, r)
                .cmdWrite(c);
            c.copyImageToBuffer(_handle, vk::ImageLayout::eTransferSrcOptimal, staging, copyRegions);
            q.submit({c});
            q.wait(c);
        }

        // read data out of the staging buffer
        auto mapped = staging.map({});
        RVI_ASSERT(mapped.size == dataSize);
        content.storage.assign(mapped.data, mapped.data + mapped.size);

        // done
        content.format = _desc.format;
        return content;
    }

    void onNameChanged() {
        const auto & name = _owner.name();
        if (_handle) setVkObjectName(_gi->device, _handle, name);
        if (_memory) setVkObjectName(_gi->device, _memory, name);
#if RAPID_VULKAN_ENABLE_VMA
        if (_allocation) vmaSetAllocationName(_gi->vmaAllocator, _allocation, name.c_str());
        (void) _allocation; // silence unused private member warning.
#endif
    }

private:
    struct SubresourceRangeComparison {
        bool operator()(const GetViewParameters & a, const GetViewParameters & b) const {
            if (a.format != b.format) return a.format < b.format;
            if (a.range.aspectMask != b.range.aspectMask) return a.range.aspectMask < b.range.aspectMask;
            if (a.range.baseMipLevel != b.range.baseMipLevel) return a.range.baseMipLevel < b.range.baseMipLevel;
            if (a.range.levelCount != b.range.levelCount) return a.range.levelCount < b.range.levelCount;
            if (a.range.baseArrayLayer != b.range.baseArrayLayer) return a.range.baseArrayLayer < b.range.baseArrayLayer;
            return a.range.layerCount < b.range.layerCount;
        }
    };

    typedef std::map<GetViewParameters, vk::ImageView, SubresourceRangeComparison> ViewMap;

private:
    Image &             _owner;
    const GlobalInfo *  _gi {};
    ConstructParameters _cp;
    Desc                _desc;
    vk::Image           _handle {};
    vk::DeviceMemory    _memory {};
    VmaAllocation       _allocation {};
    mutable ViewMap     _views;

private:
    vk::ImageViewType determineViewType(vk::ImageViewType candidate, const vk::ImageSubresourceRange & range) const {
        if (vk::ImageViewType::e1D <= candidate && candidate <= vk::ImageViewType::eCubeArray) return candidate;
        switch (_desc.type) {
        case vk::ImageType::e1D:
            if (_desc.arrayLayers > 1 && range.layerCount > 1)
                return vk::ImageViewType::e1DArray;
            else
                return vk::ImageViewType::e1D;
        case vk::ImageType::e2D: {
            bool isArray = (_desc.arrayLayers > 1 && range.layerCount > 1);
            bool isCube  = _cp.isCubeOrCubeArray() && (0 == range.baseArrayLayer && range.layerCount == VK_REMAINING_ARRAY_LAYERS);
            if (isArray)
                return isCube ? vk::ImageViewType::eCubeArray : vk::ImageViewType::e2DArray;
            else
                return isCube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D;
        }
        case vk::ImageType::e3D:
        default:
            return vk::ImageViewType::e3D;
        }
    }

    static Rect3D clampRect3D(Rect3D rect, const vk::Extent3D & extent) {
        clampRange(rect.x, rect.w, extent.width);
        clampRange(rect.y, rect.h, extent.height);
        clampRange(rect.z, rect.d, extent.depth);
        return rect;
    }

    vk::Extent3D getMipExtent(size_t level) const {
        auto extent = _desc.extent;
        for (size_t i = 0; i < level; ++i) {
            extent.width  = std::max(extent.width / 2, 1u);
            extent.height = std::max(extent.height / 2, 1u);
            extent.depth  = std::max(extent.depth / 2, 1u);
        }
        return extent;
    }

    std::vector<vk::Extent3D> buildMipExtentArray() const {
        std::vector<vk::Extent3D> result;
        result.reserve(_desc.mipLevels);
        auto extent = _desc.extent;
        for (size_t i = 0; i < _desc.mipLevels; ++i) {
            result.push_back(extent);
            extent.width  = std::max(extent.width / 2, 1u);
            extent.height = std::max(extent.height / 2, 1u);
            extent.depth  = std::max(extent.depth / 2, 1u);
        }
        return result;
    }
};

vk::ImageAspectFlags Image::determineImageAspect(vk::Format format, vk::ImageAspectFlags aspect) {
    switch (format) {
    // depth only format
    case vk::Format::eD16Unorm:
    case vk::Format::eD32Sfloat:
    case vk::Format::eX8D24UnormPack32:
        return vk::ImageAspectFlagBits::eDepth;

    // stencil only format
    case vk::Format::eS8Uint:
        return vk::ImageAspectFlagBits::eStencil;

    // depth + stencil format
    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32SfloatS8Uint:
        if (vk::ImageAspectFlagBits::eDepth == aspect || vk::ImageAspectFlagBits::eStencil == aspect)
            return aspect;
        else
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;

    // TODO: multi-planer formats

    // default format
    default:
        return vk::ImageAspectFlagBits::eColor;
    }
}

Image::Image(const ConstructParameters & cp): Root(cp) { _impl = new Impl(*this, cp); }
Image::~Image() {
    delete _impl;
    _impl = nullptr;
}
auto Image::desc() const -> const Desc & { return _impl->desc(); }
auto Image::getView(const GetViewParameters & p) const -> vk::ImageView { return _impl->getView(p); }
void Image::setContent(const SetContentParameters & p) { return _impl->setContent(p); }
auto Image::readContent(const ReadContentParameters & p) -> Content { return _impl->readContent(p); }

// *********************************************************************************************************************
// Shader
// *********************************************************************************************************************

Shader Shader::EMPTY({});

Shader::Shader(const ConstructParameters & params): Root(params), _gi(params.gi) {
    if (params.spirv.empty()) return; // Constructing an empty shader module. Not an error.
    _handle = _gi->device.createShaderModule({{}, params.spirv.size() * sizeof(uint32_t), params.spirv.data()}, _gi->allocator);
    _entry  = params.entry;
    _spirv.assign(params.spirv.begin(), params.spirv.end());
}

Shader::~Shader() { _gi->safeDestroy(_handle); }

// *********************************************************************************************************************
// Render Pass
// *********************************************************************************************************************

RenderPass::ConstructParameters & RenderPass::ConstructParameters::simple(vk::ArrayProxy<const vk::Format> colors, vk::Format depth, bool clear, bool store) {
    // initialize attachment array
    for (auto c : colors) {
        attachments.push_back(vk::AttachmentDescription()
                                  .setFormat(c)
                                  .setLoadOp(clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad)
                                  .setStoreOp(store ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare)
                                  .setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                  .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal));
    }
    if (vk::Format::eUndefined != depth) {
        auto d = vk::AttachmentDescription();
        d.setFormat(depth)
            .setLoadOp(clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad)
            .setStoreOp(store ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare)
            .setStencilLoadOp(d.loadOp)
            .setStencilStoreOp(d.storeOp)
            .setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        attachments.push_back(d);
    }

    // create subpass array
    subpasses.resize(1);
    auto & subpass0 = subpasses[0];
    for (uint32_t i = 0; i < colors.size(); ++i) subpass0.colors.push_back(vk::AttachmentReference(i, vk::ImageLayout::eColorAttachmentOptimal));
    if (vk::Format::eUndefined != depth) {
        subpass0.depth = vk::AttachmentReference((uint32_t) colors.size(), vk::ImageLayout::eDepthStencilAttachmentOptimal);
    }

    //// Create an external dependency to ensure previous rendering to color and depth buffers are done before this render pass begins.
    // dependencies.push_back(vk::SubpassDependency(
    //     VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
    //     vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
    //     vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
    //     vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead));

    // done
    return *this;
}

RenderPass::RenderPass(const ConstructParameters & cp): Root(cp), _gi(cp.gi) {
    RVI_REQUIRE(cp.subpasses.size() > 0);
    std::vector<vk::SubpassDescription> subpasses;
    for (const auto & s : cp.subpasses) {
        auto desc = vk::SubpassDescription({}, vk::PipelineBindPoint::eGraphics).setInputAttachments(s.inputs).setColorAttachments(s.colors);
        if (s.depth.has_value()) desc.setPDepthStencilAttachment(&s.depth.value());
        subpasses.push_back(desc);
    }

    vk::RenderPassCreateInfo ci(cp.flags);
    ci.setAttachments(cp.attachments);
    ci.setSubpasses(subpasses);
    ci.setDependencies(cp.dependencies);
    _handle = _gi->device.createRenderPass(ci, _gi->allocator);

#if RAPID_VULKAN_ENABLE_DEBUG_BUILD
    _cp = cp;
    (void) _cp;
#endif
}

RenderPass::~RenderPass() { _gi->safeDestroy(_handle); }

void RenderPass::cmdBegin(vk::CommandBuffer cb, vk::RenderPassBeginInfo info) const {
    info.setRenderPass(_handle);
    cb.beginRenderPass(info, vk::SubpassContents::eInline);
}

void RenderPass::cmdNext(vk::CommandBuffer cb) const { cb.nextSubpass(vk::SubpassContents::eInline); }

void RenderPass::cmdEnd(vk::CommandBuffer cb) const { cb.endRenderPass(); }

void RenderPass::onNameChanged(const std::string &) {
    if (_handle) setVkObjectName(_gi->device, _handle, name());
}

// *********************************************************************************************************************
// Framebuffer
// *********************************************************************************************************************

Framebuffer::ConstructParameters & Framebuffer::ConstructParameters::addImage(Image & image) {
    const auto & d = image.desc();
    // Can't add 3D texture as framebuffer.
    RVI_REQUIRE(d.extent.depth == 1);
    // Can't add image with mipmaps as framebuffer. If you want to that, you'll need to manually create a view
    // for one of the mipmap levels and add that view instead.
    RVI_REQUIRE(d.mipLevels == 1);
    // Verify the image's extent matches existing value when there are already attachments.
    if (attachments.size() > 0) { RVI_REQUIRE(width == d.extent.width && height == d.extent.height && layers == d.arrayLayers); }
    setExtent(d.extent.width, d.extent.height, d.arrayLayers);
    attachments.emplace_back(image.getView({}));
    return *this;
}

Framebuffer::Framebuffer(const ConstructParameters & cp): Root(cp), _gi(cp.gi) {
    vk::FramebufferCreateInfo ci({}, cp.pass);
    ci.setAttachments(cp.attachments).setWidth((uint32_t) cp.width).setHeight((uint32_t) cp.height).setLayers((uint32_t) 1);
    _handle = _gi->device.createFramebuffer(ci, _gi->allocator);
}

Framebuffer::~Framebuffer() { _gi->safeDestroy(_handle); }

void Framebuffer::onNameChanged(const std::string &) {
    if (_handle) setVkObjectName(_gi->device, _handle, name());
}

// *********************************************************************************************************************
// Argument and ArgumentPack
// *********************************************************************************************************************

class Argument::Impl {
public:
    struct BufferArgs {
        std::vector<vk::DescriptorBufferInfo> infos;
        std::vector<BufferView>               buffers;

        size_t size() const {
            RAPID_VULKAN_ASSERT(infos.size() == buffers.size());
            return infos.size();
        }
    };

    struct ImageArgs {
        enum Type {
            INVALID = 0,
            IMAGE,
            SAMPLER,
            COMBINED,
        };
        std::vector<vk::DescriptorImageInfo> infos;
        std::vector<ImageSampler>            images;
        Type                                 type = INVALID; // all args must be of the same type.

        size_t size() const {
            RAPID_VULKAN_ASSERT(infos.size() == images.size());
            return infos.size();
        }
    };

    typedef std::variant<std::monostate, BufferArgs, ImageArgs> Value;

    Impl() {
        RAPID_VULKAN_ASSERT(_value.index() == 0); // should start with monostate.
    }

    ~Impl() {}

    void b(vk::ArrayProxy<const BufferView> v) {
        auto sameValue = [&]() {
            auto p = std::get_if<BufferArgs>(&_value);
            if (!p) return false;
            if (p->size() != v.size()) return false;
            for (size_t i = 0; i < v.size(); ++i) {
                if (p->buffers[i] != v.data()[i]) return false;
            }
            return true;
        };
        if (sameValue()) return;
        _value      = BufferArgs();
        auto & args = std::get<BufferArgs>(_value);
        args.buffers.assign(v.begin(), v.end());
        args.infos.resize(args.buffers.size());
        for (size_t i = 0; i < args.buffers.size(); ++i) {
            args.infos[i].buffer = args.buffers[i].buffer;
            args.infos[i].offset = args.buffers[i].offset;
            args.infos[i].range  = args.buffers[i].size;
        }
        _timestamp.fetch_add(1);
    }

    void i(vk::ArrayProxy<const ImageSampler> v) {
        auto sameValue = [&]() {
            auto p = std::get_if<ImageArgs>(&_value);
            if (!p) return false;
            if (p->size() != v.size()) return false;
            for (size_t i = 0; i < v.size(); ++i) {
                if (p->images[i] != v.data()[i]) return false;
            }
            return true;
        };
        if (sameValue()) return;
        _value      = ImageArgs();
        auto & args = std::get<ImageArgs>(_value);
        args.images.assign(v.begin(), v.end());
        args.infos.resize(args.images.size());
        args.type = ImageArgs::INVALID;
        for (size_t i = 0; i < args.images.size(); ++i) {
            auto & info  = args.infos[i];
            auto & image = args.images[i];
            info.sampler = image.sampler;
            ImageArgs::Type t;
            if (image.imageView) {
                info.imageLayout = image.imageLayout;
                info.imageView   = image.imageView;
                t                = info.sampler ? ImageArgs::COMBINED : ImageArgs::IMAGE;
            } else {
                info.imageLayout = vk::ImageLayout::eUndefined;
                info.imageView   = nullptr;
                t                = info.sampler ? ImageArgs::SAMPLER : ImageArgs::INVALID;
            }
            if (0 == i) {
                args.type = t;
            } else if (args.type != t) {
                RVI_LOGE("All images are expected of same type. But image %zu has a different type than the first image.", i);
                args.type = ImageArgs::INVALID;
            }
        }
        _timestamp.fetch_add(1);
    }

    // void c(size_t offset, size_t size, const void * data) {
    //     auto p = std::get_if<Constants>(&_value);
    //     if (p) {
    //         auto sameValue = [&]() {
    //             if (p->size() != (offset + size)) return false;
    //             return memcmp(p->data() + offset, data, size) == 0;
    //         };
    //         if (sameValue()) return;
    //         memcpy(p->data() + offset, data, size);
    //     } else {
    //         std::vector<uint8_t> v(offset + size, 0);
    //         memcpy(v.data() + offset, data, size);
    //         _value = std::move(v);
    //     }
    //     _timestamp.fetch_add(1);
    // }

    const Value & value() const { return _value; }

    int64_t modificationTimestamp() const { return _timestamp; }

    // return number of descriptors in the argument.
    size_t count() const {
        auto index = _value.index();
        if (1 == index)
            return std::get<BufferArgs>(_value).infos.size();
        else if (2 == index)
            return std::get<ImageArgs>(_value).infos.size();
        else
            return 0;
    }

    bool typeCompatibleWith(vk::DescriptorType t) const {
        auto index = _value.index();
        if (1 == index) {
            return t == vk::DescriptorType::eStorageBuffer || t == vk::DescriptorType::eUniformBuffer || t == vk::DescriptorType::eUniformBufferDynamic ||
                   t == vk::DescriptorType::eStorageBufferDynamic;
        } else if (2 == index) {
            auto & args = std::get<ImageArgs>(_value);
            switch (args.type) {
            case ImageArgs::SAMPLER:
                return t == vk::DescriptorType::eSampler;
            case ImageArgs::IMAGE:
                return t == vk::DescriptorType::eSampledImage || t == vk::DescriptorType::eStorageImage || t == vk::DescriptorType::eInputAttachment;
            case ImageArgs::COMBINED:
                return t == vk::DescriptorType::eCombinedImageSampler;
            default:
                return false;
            }
        } else {
            return false;
        }
    }

    const char * type() const {
        auto index = _value.index();
        if (1 == index)
            return "Buffer";
        else if (2 == index) {
            auto & args = std::get<ImageArgs>(_value);
            switch (args.type) {
            case ImageArgs::IMAGE:
                return "Image";
            case ImageArgs::SAMPLER:
                return "Sampler";
            case ImageArgs::COMBINED:
                return "Combined";
            default:
                return "<InvalidImageSampler>";
            }
        } else
            return "<None>";
    }

private:
    Value                _value;
    std::atomic<int64_t> _timestamp = 0;
};

Argument::Argument(): _impl(new Impl()) {}
Argument::~Argument() {
    delete _impl;
    _impl = nullptr;
}
Argument & Argument::b(vk::ArrayProxy<const BufferView> v) {
    _impl->b(v);
    return *this;
}
Argument & Argument::i(vk::ArrayProxy<const ImageSampler> v) {
    _impl->i(v);
    return *this;
}

class ArgumentImpl : public Argument {
public:
    ArgumentImpl() {}
    ~ArgumentImpl() {}
};

class ArgumentPack::Impl {
public:
    Impl(ArgumentPack &) {}

    ~Impl() {}

    void clear() {
        _descriptors.clear();
        _constants.clear();
    }

    void set(DescriptorIdentifier id, vk::ArrayProxy<const BufferView> v) { _descriptors[id].b(v); }

    void set(DescriptorIdentifier id, vk::ArrayProxy<const ImageSampler> v) { _descriptors[id].i(v); }

    void set(size_t offset, size_t size, const void * data, vk::ShaderStageFlags stages) {
        if (0 == data || 0 == size || !stages) return; // ignore empty data.
        _constants.push_back({stages, (uint32_t) offset});
        _constants.back().value.assign((const uint8_t *) data, (const uint8_t *) data + size);
    }

    Argument * get(DescriptorIdentifier id) { return &_descriptors[id]; }

    const Argument * find(DescriptorIdentifier id) const {
        auto iter = _descriptors.find(id);
        return iter == _descriptors.end() ? nullptr : &iter->second;
    }

    std::vector<std::tuple<vk::PushConstantRange, const void *>> getConstant(vk::ShaderStageFlags stages, uint32_t begin, uint32_t end) const {
        if (!stages) return {};
        if (begin >= end) return {};
        std::vector<std::tuple<vk::PushConstantRange, const void *>> v;
        for (auto & c : _constants) {
            vk::PushConstantRange range {};
            range.stageFlags = c.stages & stages;
            if (!range.stageFlags) continue;
            range.offset = std::max(c.offset, begin);
            end          = std::min((uint32_t) (c.offset + c.value.size()), end);
            if (range.offset >= end) continue;
            range.size = end - range.offset;
            v.push_back({range, c.value.data() + range.offset - c.offset});
        }
        return v;
    }

private:
    struct ConstantArgument {
        vk::ShaderStageFlags stages {};
        uint32_t             offset {};
        std::vector<uint8_t> value {};
    };

    std::unordered_map<DescriptorIdentifier, ArgumentImpl> _descriptors;
    std::vector<ConstantArgument>                          _constants;
};

ArgumentPack::ArgumentPack(const ConstructParameters & cp): Root(cp) { _impl = new Impl(*this); }
ArgumentPack::~ArgumentPack() {
    delete _impl;
    _impl = nullptr;
}
auto ArgumentPack::clear() -> ArgumentPack & {
    _impl->clear();
    return *this;
}
auto ArgumentPack::b(DescriptorIdentifier id, vk::ArrayProxy<const BufferView> v) -> ArgumentPack & {
    _impl->set(id, v);
    return *this;
}
auto ArgumentPack::i(DescriptorIdentifier id, vk::ArrayProxy<const ImageSampler> v) -> ArgumentPack & {
    _impl->set(id, v);
    return *this;
}
auto ArgumentPack::c(size_t offset, size_t size, const void * data, vk::ShaderStageFlags stages) -> ArgumentPack & {
    _impl->set(offset, size, data, stages);
    return *this;
}
auto ArgumentPack::get(DescriptorIdentifier id) -> Argument * { return _impl->get(id); }
auto ArgumentPack::find(DescriptorIdentifier id) const -> const Argument * { return _impl->find(id); }

// *********************************************************************************************************************
// Pipeline Reflection
// *********************************************************************************************************************

template<typename T, typename FUNC, typename... ARGS>
static std::vector<T *> enumerateShaderVariables(SpvReflectShaderModule & module, FUNC func, ARGS... args) {
    uint32_t count  = 0;
    auto     result = func(&module, args..., &count, nullptr);
    RVI_REQUIRE(result == SPV_REFLECT_RESULT_SUCCESS);
    std::vector<T *> v(count);
    result = func(&module, args..., &count, v.data());
    RVI_REQUIRE(result == SPV_REFLECT_RESULT_SUCCESS);
    return v;
}

struct MergedDescriptorBinding {
    SpvReflectDescriptorBinding * binding    = nullptr;
    VkShaderStageFlags            stageFlags = 0;
    std::set<std::string>         names;
};

struct MergedDescriptorSet {
    std::map<uint32_t, MergedDescriptorBinding> descriptors; // key is binding location
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
        auto &       d    = merged.descriptors[i->binding];
        if (d.binding) {
            RVI_ASSERT(d.binding->binding == i->binding);
            // check for possible conflict
            if (d.binding->descriptor_type != i->descriptor_type)
                RVI_LOGE("Shader variable %s has conflicting types: %d != %d", name, d.binding->descriptor_type, i->descriptor_type);
            // TODO: check array.dims_count and array.dims
        } else {
            d.binding = i;
        }
        d.stageFlags |= module.shader_stage;
        d.names.insert(name);
    }
}

static PipelineReflection::Descriptor convertArray(const MergedDescriptorBinding & src) {
    PipelineReflection::Descriptor dst = {};
    dst.names                          = src.names;
    dst.binding.binding                = src.binding->binding; // wtf, so many bindings ...
    dst.binding.descriptorType         = static_cast<vk::DescriptorType>(src.binding->descriptor_type);
    dst.binding.descriptorCount        = 1;
    for (uint32_t i = 0; i < src.binding->array.dims_count; ++i) dst.binding.descriptorCount *= src.binding->array.dims[i];
    dst.binding.stageFlags = (vk::ShaderStageFlagBits) src.stageFlags;
    return dst;
}

static PipelineReflection::DescriptorSet convertSet(const MergedDescriptorSet & merged) {
    PipelineReflection::DescriptorSet set;
    for (const auto & kv : merged.descriptors) {
        if (set.size() <= kv.first) set.resize(kv.first + 1);
        set[kv.first] = convertArray(kv.second);
    }
    return set;
}

static PipelineReflection convertRefl(std::map<uint32_t, MergedDescriptorSet> & descriptors) {
    PipelineReflection refl;
    if (!descriptors.empty()) {
        refl.descriptors.resize(descriptors.rbegin()->first + 1);
        for (const auto & kv : descriptors) {
            RVI_ASSERT(kv.first < refl.descriptors.size());
            refl.descriptors[kv.first] = convertSet(kv.second);
        }
    }
    return refl;
}

static void convertVertexInputs(PipelineReflection & refl, vk::ArrayProxy<SpvReflectInterfaceVariable *> vertexInputs) {
    for (auto i : vertexInputs) {
        auto name = std::string(i->name);
        if (name.substr(0, 3) == "gl_") continue; // skip OpenGL's reserved inputs.
        refl.vertex[i->location] = {(vk::Format) i->format, name};
    }
}

static PipelineReflection reflectShaders(const std::string & pipelineName, vk::ArrayProxy<const Shader * const> shaders) {
    RVI_ASSERT(!shaders.empty());

    // The first uint32_t is set index. The 2nd one is shader variable name.
    std::map<uint32_t, MergedDescriptorSet> merged;

    std::vector<SpvReflectInterfaceVariable *> vertexInputs;

    PipelineReflection::ConstantLayout constants;

    std::vector<SpvReflectShaderModule> modules;

    for (const auto & shader : shaders) {
        // ignore null shader pointer
        if (!shader) continue;

        // Ignore shader w/o spirv code
        auto spirv = shader->spirv();
        if (spirv.empty()) continue;

        SpvReflectShaderModule module;
        SpvReflectResult       result = spvReflectCreateShaderModule(spirv.size() * sizeof(uint32_t), spirv.data(), &module);
        RVI_REQUIRE(result == SPV_REFLECT_RESULT_SUCCESS);

        // Extract descriptor sets from each shader, then merge together.
        auto sets = enumerateShaderVariables<SpvReflectDescriptorSet>(module, spvReflectEnumerateEntryPointDescriptorSets, shader->entry().c_str());
        for (const auto & set : sets) mergeDescriptorSet(merged[set->set], module, {set->binding_count, set->bindings});

        // enumerate push constants
        auto pc = enumerateShaderVariables<SpvReflectBlockVariable>(module, spvReflectEnumeratePushConstantBlocks);
        for (const auto & c : pc) {
            auto & sc = constants[(vk::ShaderStageFlagBits) module.shader_stage];
            sc.begin  = std::min(sc.begin, (uint32_t) c->offset);
            sc.end    = std::max(sc.end, (uint32_t) (c->offset + c->size));
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

class PipelineLayout::Impl {
public:
    Impl(PipelineLayout & owner, vk::ArrayProxy<const Shader * const> shaders): _owner(owner) {
        // make sure shader array is not empty and the first shader is not null.
        RVI_REQUIRE(shaders.size() > 0 && shaders.front());

        _gi         = shaders.front()->gi();
        _reflection = reflectShaders(owner.name(), shaders);

        // create descriptor set
        _sets.resize(_reflection.descriptors.size());
        for (uint32_t s = 0; s < _sets.size(); ++s) {
            // go through all descriptors in this set. build variable and binding arrays.
            auto & bindings = _sets[s].bindings;
            auto   sizesMap = std::map<vk::DescriptorType, uint32_t>();
            bindings.reserve(_reflection.descriptors[s].size());
            for (uint32_t i = 0; i < _reflection.descriptors[s].size(); ++i) {
                const auto & d = _reflection.descriptors[s][i];
                if (d.empty()) continue; // skip empty descriptor
                bindings.push_back(d.binding);
                sizesMap[d.binding.descriptorType] += d.binding.descriptorCount;
            }

            // we have gone through all descriptors in the set. Now we can create descriptor set layout.
            auto ci         = vk::DescriptorSetLayoutCreateInfo();
            ci.bindingCount = (uint32_t) bindings.size();
            ci.pBindings    = bindings.data();
            _sets[s].layout = _gi->device.createDescriptorSetLayout(ci, _gi->allocator);

            // convert sizes map to array
            std::vector<vk::DescriptorPoolSize> sizes;
            sizes.reserve(sizesMap.size());
            for (const auto & kv : sizesMap) sizes.push_back({kv.first, kv.second});

            // Create descriptor pool
            // TODO: make the pool size configurable.
            _sets[s].pool          = _gi->device.createDescriptorPool(vk::DescriptorPoolCreateInfo().setMaxSets(1024).setPoolSizes(sizes), _gi->allocator);
            _sets[s].availableSets = 1024;
        }

        // create push constant array
        std::vector<vk::PushConstantRange> pc;
        pc.reserve(_reflection.constants.size());
        for (const auto & kv : _reflection.constants) {
            if (kv.second.empty()) continue;
            pc.push_back({kv.first, kv.second.begin, kv.second.end - kv.second.begin});
        }

        // create pipeline layout array
        std::vector<vk::DescriptorSetLayout> layouts;
        layouts.reserve(_sets.size());
        for (const auto & s : _sets) layouts.push_back(s.layout);

        // create pipeline layout
        _handle = _gi->device.createPipelineLayout(vk::PipelineLayoutCreateInfo().setSetLayouts(layouts).setPushConstantRanges(pc), _gi->allocator);

        // done
        onNameChanged();
    }

    ~Impl() {
        for (auto & s : _sets) { s.clear(*_gi); }
        _sets.clear();
        _gi->safeDestroy(_handle);
    }

    vk::PipelineLayout handle() const { return _handle; }

    const PipelineReflection & reflection() const { return _reflection; }

    bool cmdBind(vk::CommandBuffer cb, vk::PipelineBindPoint bp, const ArgumentPack & ap) { return bindDescriptors(cb, bp, ap) && bindPushConstants(cb, ap); }

    void onNameChanged() {
        if (_handle) setVkObjectName(_gi->device, _handle, _owner.name());
    }

private:
    struct DescSet {
        vk::DescriptorSetLayout                     layout;
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        vk::DescriptorPool                          pool;
        size_t                                      availableSets = 0; ///< number of sets that are available for allocation in the pool.
        std::vector<vk::DescriptorPool>             full;              // pools that are full already.

        vk::DescriptorSet alloc(const GlobalInfo & gi) {
            if (availableSets > 0) {
                auto s = gi.device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(pool, 1, &layout));
                availableSets--;
                return s[0];
            } else {
                RVI_LOGE("descriptor pool is out of space.");
                return {};
            }
        }

        void clear(const GlobalInfo & gi) {
            gi.safeDestroy(layout);
            gi.safeDestroy(pool);
            for (auto & p : full) { gi.safeDestroy(p); }
            full.clear();
        }
    };

private:
    PipelineLayout &     _owner;
    const GlobalInfo *   _gi = nullptr;
    PipelineReflection   _reflection;
    std::vector<DescSet> _sets;
    vk::PipelineLayout   _handle;

    static const Argument::Impl * findArgument(const ArgumentPack & ap, uint32_t set, uint32_t binding) {
        auto a = ap.find({set, binding});
        return a ? a->_impl : nullptr;
    }

    bool bindDescriptors(vk::CommandBuffer cb, vk::PipelineBindPoint bindPoint, const ArgumentPack & ap) {
        auto writes     = std::vector<vk::WriteDescriptorSet>();
        auto setHandles = std::vector<vk::DescriptorSet>();
        for (uint32_t si = 0; si < _sets.size(); ++si) {
            auto & s       = _sets[si];
            auto   descSet = s.alloc(*_gi);
            if (!descSet) return false;
            setHandles.push_back(descSet);
            for (uint32_t i = 0; i < s.bindings.size(); ++i) {
                auto & b = s.bindings[i];
                auto & w = writes.emplace_back();
                w.setDstSet(descSet);
                w.setDstBinding(b.binding);
                w.setDescriptorType(b.descriptorType);

                // Locate the arugment in the pack for this binding slot.
                const Argument::Impl * a = findArgument(ap, si, i);
                if (!a) {
                    RVI_LOGE("Failed to bind argument pack (%s) to pipeline layout (%s): set %u slot %u not found in the argument pack.", ap.name().c_str(),
                             _owner.name().c_str(), si, i);
                    return false;
                }

                // verify that the argument type is compatible with the descriptor type
                if (!a->typeCompatibleWith(b.descriptorType)) {
                    RVI_LOGE("Failed to bind argument pack (%s) to pipeline layout (%s): set %u slot %u is of type %s, but the argument is of type %s.",
                             ap.name().c_str(), _owner.name().c_str(), si, i, vk::to_string(b.descriptorType).c_str(), a->type());
                    return false;
                }

                // verify that there're enough descriptors in the argument.
                if (a->count() < b.descriptorCount) {
                    RVI_LOGE(
                        "Failed to bind argument pack (%s) to pipeline layout (%s): set %u slot %u requires %u descriptors, but the argument has only %zu.",
                        ap.name().c_str(), _owner.name().c_str(), si, i, b.descriptorCount, a->count());
                    return false;
                }

                auto & value = a->value();
                if (auto buf = std::get_if<Argument::Impl::BufferArgs>(&value))
                    w.setBufferInfo(buf->infos);
                else if (auto img = std::get_if<Argument::Impl::ImageArgs>(&value))
                    w.setImageInfo(img->infos);
                else {
                    // we should not reach here, since we have already checked the type compatibility.
                    RAPID_VULKAN_ASSERT(false, "should never reach here.");
                }
            }
        }
        _gi->device.updateDescriptorSets(writes, {});
        cb.bindDescriptorSets(bindPoint, _handle, 0, setHandles, {});
        return true;
    }

    bool bindPushConstants(vk::CommandBuffer cb, const ArgumentPack & ap) {
        for (const auto & kv : _reflection.constants) {
            if (kv.second.empty()) continue;
            auto v = ap._impl->getConstant(kv.first, kv.second.begin, kv.second.end);
            if (v.empty()) {
                // No warnings ehre. Push constant value is persistent within one
                // command buffer submit. So it is not required to update all the values on every pipeline binding.
                continue;
            }
            for (const auto & [pcr, data] : v) cb.pushConstants(_handle, pcr.stageFlags, pcr.offset, pcr.size, data);
        }
        return true;
    }
};

PipelineLayout::PipelineLayout(const ConstructParameters & cp): Root(cp) { _impl = new Impl(*this, cp.shaders); }
PipelineLayout::~PipelineLayout() {
    delete _impl;
    _impl = nullptr;
}
auto PipelineLayout::handle() const -> vk::PipelineLayout { return _impl->handle(); }
auto PipelineLayout::reflection() const -> const PipelineReflection & { return _impl->reflection(); }
bool PipelineLayout::cmdBind(vk::CommandBuffer cb, vk::PipelineBindPoint bp, const ArgumentPack & ap) const { return _impl->cmdBind(cb, bp, ap); }
void PipelineLayout::onNameChanged(const std::string &) { _impl->onNameChanged(); }

// *********************************************************************************************************************
// Pipeline
// *********************************************************************************************************************

class Pipeline::Impl {
public:
    vk::Pipeline          handle {};
    vk::PipelineBindPoint bindPoint;

    Impl(Pipeline & owner, vk::ArrayProxy<const Shader * const> shaders) {
        // The first shader must be valid.
        RVI_REQUIRE(shaders.size() > 0 && shaders.front(), "Failed to create pipeline layout: the first shader in the shader array must be valid.");
        _gi = shaders.front()->gi();
        // TODO: reuse layout via a cache object?
        _layout.reset(new PipelineLayout({{owner.name()}, shaders}));
    }

    ~Impl() { _gi->device.destroy(handle); }

    PipelineLayout & layout() const { return *_layout; }

    void cmdBind(vk::CommandBuffer cb, const ArgumentPack & args) const {
        if (handle) _layout->cmdBind(cb, bindPoint, args);
    }

private:
    const GlobalInfo *  _gi = nullptr;
    Ref<PipelineLayout> _layout;
};

Pipeline::Pipeline(const std::string & name, vk::ArrayProxy<const Shader * const> shaders): Root({name}) { _impl = new Impl(*this, shaders); }
Pipeline::~Pipeline() {
    delete _impl;
    _impl = nullptr;
}
auto Pipeline::layout() const -> const PipelineLayout & { return _impl->layout(); }
void Pipeline::cmdBind(vk::CommandBuffer cb, const ArgumentPack & ap) const { return _impl->cmdBind(cb, ap); }

// *********************************************************************************************************************
// Graphics Pipeline
// *********************************************************************************************************************

GraphicsPipeline::GraphicsPipeline(const ConstructParameters & params): Pipeline(params.name, {params.vs, params.fs}) {
    // create shader stage array
    RVI_REQUIRE(params.vs, "Vertex shader is required for graphics pipeline.");
    auto gi           = params.vs->gi();
    auto shaderStages = std::vector<vk::PipelineShaderStageCreateInfo>();
    shaderStages.push_back({{}, vk::ShaderStageFlagBits::eVertex, params.vs->handle(), params.vs->entry().c_str()});
    if (params.fs) shaderStages.push_back({{}, vk::ShaderStageFlagBits::eFragment, params.fs->handle(), params.fs->entry().c_str()});

    // setup vertex input stage
    const auto & refl = layout().reflection();
    if (refl.vertex.size() != params.va.size()) {
        RVI_LOGE("Failed to create graphics pipeline (%s): vertex input stage requires %zu attributes, but only %zu are provided.", params.name.c_str(),
                 refl.vertex.size(), params.va.size());
        return;
    }
    for (const auto & kv : refl.vertex) {
        auto location = kv.first;
        auto iter     = std::find_if(params.va.begin(), params.va.end(), [location](const auto & a) { return a.location == location; });
        if (iter == params.va.end()) {
            RVI_LOGE("Failed to create graphics pipeline (%s): vertex input stage requires attribute at location %u, but it is not provided.",
                     params.name.c_str(), location);
            return;
        }
        const auto & attribute = *iter;
        auto         vbi       = std::find_if(params.vb.begin(), params.vb.end(), [attribute](const auto & b) { return b.binding == attribute.binding; });
        if (vbi == params.vb.end()) {
            RVI_LOGE("Failed to create graphics pipeline (%s): vertex input stage requires vertex buffer #%u, but it is not provided.", params.name.c_str(),
                     attribute.binding);
            return;
        }
    }
    auto vertex = vk::PipelineVertexInputStateCreateInfo().setVertexAttributeDescriptions(params.va).setVertexBindingDescriptions(params.vb);

    // setup viewport and scissor states.
    auto viewport = vk::PipelineViewportStateCreateInfo();
    viewport.setViewports(params.viewports);
    viewport.setScissors(params.scissors);

    // setup dynamic states
    std::vector<vk::DynamicState> dynamicStates;
    for (const auto & [s, v] : params.dynamic) {
        dynamicStates.push_back(s);
        switch (s) {
        case vk::DynamicState::eViewport:
            viewport.setViewportCount((uint32_t) v);
            viewport.setPViewports(nullptr);
            break;
        case vk::DynamicState::eViewportWithCount:
            viewport.setViewports({});
            break;
        case vk::DynamicState::eScissor:
            viewport.setScissorCount((uint32_t) v);
            viewport.setPScissors(nullptr);
            break;
        case vk::DynamicState::eScissorWithCount:
            viewport.setScissors({});
            break;
        default:
            // do nothing
            break;
        }
    }
    vk::PipelineDynamicStateCreateInfo dynamicCI({}, dynamicStates);

    // setup blend stage
    auto blend           = vk::PipelineColorBlendStateCreateInfo {}.setAttachments(params.attachments);
    blend.blendConstants = params.blendConstants;

    // setup the create info
    auto ci = vk::GraphicsPipelineCreateInfo({}, (uint32_t) shaderStages.size(), shaderStages.data(), &vertex, &params.ia, &params.tess, &viewport,
                                             &params.rast, &params.msaa, &params.depth, &blend, &dynamicCI, layout().handle(), params.pass, params.subpass,
                                             params.baseHandle, params.baseIndex);

    // create the shader.
    _impl->handle    = gi->device.createGraphicsPipeline(nullptr, ci, gi->allocator).value;
    _impl->bindPoint = vk::PipelineBindPoint::eGraphics;
}

void GraphicsPipeline::cmdDraw(vk::CommandBuffer cb, const DrawParameters & dp) {
    if (!_impl->handle) return;

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, _impl->handle);

    // bind vertex buffers
    if (dp.vertexBuffers.size() > 0) {
        RVI_REQUIRE(dp.vertexBuffers.size() <= 16); // TODO: use a stack_array class to remove this limitation.
        vk::Buffer     buffers[16];
        vk::DeviceSize offsets[16];
        for (size_t i = 0; i < dp.vertexBuffers.size(); ++i) {
            const auto & view = dp.vertexBuffers.data()[i];
            RVI_REQUIRE(view.buffer, "Empty vertex buffer is not allowed.");
            buffers[i] = view.buffer;
            offsets[i] = view.offset;
            // TODO: validate vertex buffer size on debug build.
        }
        cb.bindVertexBuffers(0, (uint32_t) dp.vertexBuffers.size(), buffers, offsets);
    }

    if (dp.indexBuffer.buffer) {
        // indexed draw
        cb.bindIndexBuffer(dp.indexBuffer.buffer, dp.indexBuffer.offset, dp.indexBuffer.indexType);
        cb.drawIndexed(dp.indexCount, dp.instanceCount, dp.firstIndex, dp.vertexOffset, dp.firstInstance);
    } else {
        // non-indexed draw
        cb.draw(dp.vertexCount, dp.instanceCount, dp.firstVertex, dp.firstInstance);
    }
}

// *********************************************************************************************************************
// Compute Pipeline
// *********************************************************************************************************************

ComputePipeline::ComputePipeline(const ConstructParameters & params): Pipeline(params.name, {params.cs}) {
    vk::ComputePipelineCreateInfo ci;
    ci.setStage({{}, vk::ShaderStageFlagBits::eCompute, params.cs->handle(), params.cs->entry().c_str()});
    ci.setLayout(layout().handle());
    auto gi          = params.cs->gi();
    _impl->handle    = gi->device.createComputePipeline(nullptr, ci, gi->allocator).value;
    _impl->bindPoint = vk::PipelineBindPoint::eCompute;
}

void ComputePipeline::cmdDispatch(vk::CommandBuffer cb, const DispatchParameters & dp) {
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, _impl->handle);
    cb.dispatch((uint32_t) dp.width, (uint32_t) dp.height, (uint32_t) dp.depth);
}

// *********************************************************************************************************************
// Swapchain
// *********************************************************************************************************************

Swapchain::ConstructParameters & Swapchain::ConstructParameters::setDevice(const Device & d) {
    gi                  = d.gi();
    surface             = d.surface();
    graphicsQueueFamily = d.graphics()->family();
    graphicsQueueIndex  = d.graphics()->index();
    if (d.present()) {
        presentQueueFamily = d.present()->family();
        presentQueueIndex  = d.present()->index();
    } else {
        presentQueueFamily = VK_QUEUE_FAMILY_IGNORED;
        presentQueueIndex  = 0;
    }
    return *this;
}

class Swapchain::Impl {
public:
    Impl(Swapchain &, const ConstructParameters & cp): _cp(cp) {
        RVI_REQUIRE(cp.gi);

        if (cp.surface) {
            constructWindowSwapchain();
        } else {
            constructHeadlessSwapchain();
        }
    }

    ~Impl() {
        clearSwapchain();
        _renderPass.reset();
        _graphicsQueue.reset();
        _presentQueue = nullptr;
    }

    const RenderPass & renderPass() const { return *_renderPass; }

    void cmdBeginBuiltInRenderPass(vk::CommandBuffer cb, const BeginRenderPassParameters & params) {
        auto bb = currentFrame().backbuffer;

        // transition back buffer layout if necessary.
        if (params.backbufferStatus.layout != DESIRED_PRESENT_STATUS.layout) {
            Barrier()
                .i(bb->image, params.backbufferStatus.access, DESIRED_PRESENT_STATUS.access, params.backbufferStatus.layout, DESIRED_PRESENT_STATUS.layout,
                   vk::ImageAspectFlagBits::eColor)
                .s(params.backbufferStatus.stages, DESIRED_PRESENT_STATUS.stages)
                .cmdWrite(cb);
        }

        // set dynamic viewport and scissor
        vk::Viewport vp(0, 0, (float) bb->extent.width, (float) bb->extent.height, 0, 1);
        // cb.setViewportWithCount(1, &vp); // FIXME: this line crashes on Ubunut. Reason unknown.
        cb.setViewport(0, 1, &vp);

        vk::Rect2D scissor({0, 0}, bb->extent);
        // cb.setScissorWithCount(1, &scissor); // FIXME: this line crashes on Ubunut. Reason unknown.
        cb.setScissor(0, 1, &scissor);

        std::array cv = {vk::ClearValue().setColor(params.color), vk::ClearValue().setDepthStencil(params.depth)};
        _renderPass->cmdBegin(cb, vk::RenderPassBeginInfo {{}, bb->fb->handle(), vk::Rect2D({0, 0}, bb->extent)}.setClearValues(cv));
    }

    void cmdEndBuiltInRenderPass(vk::CommandBuffer cb) {
        _renderPass->cmdEnd(cb);
        auto bb    = (Backbuffer *) currentFrame().backbuffer;
        bb->status = {vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits::eMemoryRead, vk::PipelineStageFlagBits::eBottomOfPipe};
    }

    const Frame & currentFrame() const { return _frames[_frameIndex % std::size(_frames)]; }

    void present(const PresentParameters & pp) {
        auto & frame = (FrameImpl &) currentFrame();
        auto   bb    = (Backbuffer *) frame.backbuffer;

        // end the frame, optionally transition the backbuffer image to present layout.
        auto cb = _graphicsQueue->begin("frame end");
        if (pp.backbufferStatus.layout != DESIRED_PRESENT_STATUS.layout) {
            Barrier()
                .i(bb->image, pp.backbufferStatus.access, DESIRED_PRESENT_STATUS.access, pp.backbufferStatus.layout, DESIRED_PRESENT_STATUS.layout,
                   vk::ImageAspectFlagBits::eColor)
                .s(pp.backbufferStatus.stages, DESIRED_PRESENT_STATUS.stages)
                .cmdWrite(cb);
            bb->status = DESIRED_PRESENT_STATUS;
        } else {
            bb->status = pp.backbufferStatus;
        }

        // present current frame
        if (_handle) {
            _graphicsQueue->submit({cb, {}, {frame.renderFinished}, {frame.frameEndSemaphore}});

            auto presentInfo = vk::PresentInfoKHR()
                                   .setSwapchainCount(1)
                                   .setPSwapchains(&_handle)
                                   .setPImageIndices(&frame.imageIndex)
                                   .setWaitSemaphoreCount(1)
                                   .setPWaitSemaphores(&frame.frameEndSemaphore);
            auto result = _presentQueue.presentKHR(&presentInfo);
            if (vk::Result::eSuccess == result) {
                // Store the command buffer. This will be used later to wait for the frame to be available for reuse.
                frame.frameEndCommands = cb;
            } else if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR) {
                // this means window/surface size is changed, we need to recreate the swapchain.
                _graphicsQueue->wait();
                recreateWindowSwapchain();
            } else {
                RVI_THROW("Failed to present swapchain image. result = %s", vk::to_string((vk::Result) result).c_str());
            }
        } else {
            // headless swapchain.
            _graphicsQueue->submit({cb, {}, {frame.renderFinished}, {frame.imageAvailable}});
            frame.frameEndCommands = cb;
        }

        // then start a new frame.
        ++_frameIndex;
        beginFrame();
    }

private:
    struct FrameImpl : public Frame {
        uint32_t          imageIndex {};
        vk::Semaphore     frameEndSemaphore {};
        vk::CommandBuffer frameEndCommands {};
        Ref<Image>        headlessImage; ///< the image that is used as the backbuffer for headless swapchain.
    };

private:
    ConstructParameters _cp;
    Ref<RenderPass>     _renderPass;
    uint64_t            _frameIndex = 0;
    vk::Queue           _presentQueue;
    Ref<CommandQueue>   _graphicsQueue;

    // the following are data members that will be cleared and recreated when swapchain is recreated.
    std::vector<FrameImpl>  _frames;
    vk::SwapchainKHR        _handle;
    std::vector<Backbuffer> _backbuffers;
    Ref<Image>              _depthBuffer;

    inline static constexpr BackbufferStatus DESIRED_PRESENT_STATUS = PresentParameters().backbufferStatus;

private:
    void constructWindowSwapchain() {
        // Construct a CommandQueue instance for graphics queue.
        RVI_REQUIRE(_cp.graphicsQueueFamily != VK_QUEUE_FAMILY_IGNORED);
        _graphicsQueue.reset(
            new CommandQueue(CommandQueue::ConstructParameters {{"swapchain graphics queue"}, _cp.gi, _cp.graphicsQueueFamily, _cp.graphicsQueueIndex}));

        // retrieve present queue handle.
        if (_cp.presentQueueFamily != VK_QUEUE_FAMILY_IGNORED) {
            _presentQueue = _cp.gi->device.getQueue(_cp.presentQueueFamily, _cp.presentQueueIndex);
            RVI_REQUIRE(_presentQueue);
        } else {
            // use graphics queue as present queue.
            _presentQueue = _graphicsQueue->handle();
        }

        // check if the back buffer format is supported.
        auto supportedFormats = _cp.gi->physical.getSurfaceFormatsKHR(_cp.surface);
        if (_cp.backbufferFormat == vk::Format::eUndefined) {
            // use the first supported format.
            RVI_REQUIRE(supportedFormats.size() > 0);
            _cp.backbufferFormat = supportedFormats[0].format;
        } else {
            // check if the specified format is supported.
            bool supported = false;
            for (auto f : supportedFormats) {
                if (f.format == _cp.backbufferFormat) {
                    supported = true;
                    break;
                }
            }
            RVI_REQUIRE(supported, "The specified back buffer format is not supported.");
        }

        // create the built-in render pass (after the back buffer format is determined)
        auto params = RenderPass::ConstructParameters {{"swapchain built-in render pass"}, _cp.gi}.simple({_cp.backbufferFormat}, _cp.depthStencilFormat);
        // We want the color buffer be in presentable layout before and after the render pass. So it can seamlessly connected with the present() call.
        params.attachments[0].setInitialLayout(DESIRED_PRESENT_STATUS.layout).setFinalLayout(DESIRED_PRESENT_STATUS.layout);
        _renderPass.reset(new RenderPass(params));

        recreateWindowSwapchain();

        // begin the first frame.
        beginFrame();
    }

    void constructHeadlessSwapchain() {
        RVI_REQUIRE(_cp.graphicsQueueFamily != VK_QUEUE_FAMILY_IGNORED);
        _graphicsQueue.reset(
            new CommandQueue(CommandQueue::ConstructParameters {{"swapchain graphics queue"}, _cp.gi, _cp.graphicsQueueFamily, _cp.graphicsQueueIndex}));

        // check back buffer format
        if (_cp.backbufferFormat == vk::Format::eUndefined) { _cp.backbufferFormat = vk::Format::eR8G8B8A8Unorm; }

        // create the built-in render pass (after the back buffer format is determined)
        auto params = RenderPass::ConstructParameters {{"swapchain built-in render pass"}, _cp.gi}.simple({_cp.backbufferFormat}, _cp.depthStencilFormat);
        // We want the color buffer be in presentable layout before and after the render pass. So it can seamlessly connected with the present() call.
        params.attachments[0].setInitialLayout(DESIRED_PRESENT_STATUS.layout).setFinalLayout(DESIRED_PRESENT_STATUS.layout);
        _renderPass.reset(new RenderPass(params));

        recreateHeadlessSwapchain();

        // begin the first frame.
        beginFrame();
    }

    void clearSwapchain() {
        for (auto & bb : _backbuffers) {
            bb.fb.clear();
            _cp.gi->safeDestroy(bb.view);
        }
        _backbuffers.clear();
        _depthBuffer.clear();
        _cp.gi->safeDestroy(_handle);
        for (auto & f : _frames) {
            _cp.gi->safeDestroy(f.imageAvailable);
            _cp.gi->safeDestroy(f.renderFinished);
            _cp.gi->safeDestroy(f.frameEndSemaphore);
        }
        _frames.clear();
    }

    void recreateWindowSwapchain() {
        clearSwapchain();
        auto gi = _cp.gi;

        // determine the swapchain size
        auto surfaceCaps = gi->physical.getSurfaceCapabilitiesKHR(_cp.surface);
        auto w           = (uint32_t) _cp.width;
        auto h           = (uint32_t) _cp.height;
        if (0 == w) w = (uint32_t) surfaceCaps.currentExtent.width;
        if (0 == h) h = (uint32_t) surfaceCaps.currentExtent.height;
        RVI_LOGI("Swapchain resolution = %ux%u", w, h);

        // if present and graphics queue are different, we need to add both of them to the queue list.
        std::vector<uint32_t> queueIndices;
        if (_cp.graphicsQueueFamily != _cp.presentQueueFamily) {
            queueIndices.push_back(_cp.graphicsQueueFamily);
            queueIndices.push_back(_cp.presentQueueFamily);
        }

        // Determine image count. Added 1 to minimal account to allow at least one GPU frame in flight.
        uint32_t backbufferCount = std::max(surfaceCaps.minImageCount + 1,
                                            std::min<uint32_t>((uint32_t) _cp.maxFramesInFlight + surfaceCaps.minImageCount, surfaceCaps.maxImageCount));

        // Select an supported alpha composite flag
        vk::CompositeAlphaFlagBitsKHR compositeAlpha;
        if (vk::CompositeAlphaFlagBitsKHR::eOpaque & surfaceCaps.supportedCompositeAlpha)
            compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        else if (vk::CompositeAlphaFlagBitsKHR::ePreMultiplied & surfaceCaps.supportedCompositeAlpha)
            compositeAlpha = vk::CompositeAlphaFlagBitsKHR::ePreMultiplied;
        else if (vk::CompositeAlphaFlagBitsKHR::ePostMultiplied & surfaceCaps.supportedCompositeAlpha)
            compositeAlpha = vk::CompositeAlphaFlagBitsKHR::ePostMultiplied;
        else if (vk::CompositeAlphaFlagBitsKHR::eInherit & surfaceCaps.supportedCompositeAlpha)
            compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
        else
            RVI_THROW("Can't find a good alpha composite flag.");

        // create the swapchain
        auto swapchainCreateInfo =
            vk::SwapchainCreateInfoKHR()
                .setSurface(_cp.surface)
                .setMinImageCount(backbufferCount)
                .setImageFormat(_cp.backbufferFormat)
                .setImageExtent({w, h})
                .setImageArrayLayers(1)
                .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst)
                .setImageSharingMode(queueIndices.empty() ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent)
                .setQueueFamilyIndices(queueIndices)
                .setCompositeAlpha(compositeAlpha)
                .setPresentMode(_cp.vsync ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eImmediate)
                .setClipped(true)
                .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity); // TODO: pass in surfaceCaps.currentTransform here and handle rotation directly in
                                                                              // rendering code.
        _handle = gi->device.createSwapchainKHR(swapchainCreateInfo, gi->allocator);

        // acquire swapchain images
        auto              images = gi->device.getSwapchainImagesKHR(_handle);
        std::stringstream ss;
        ss << "Swapchain created with " << images.size() << " images: ";
        for (const auto & i : images) { ss << " " << std::hex << i; }
        RVI_LOGI("%s", ss.str().c_str());

        // create a graphics command buffer to transfer swapchain images to the right layout.
        auto c = _graphicsQueue->begin("transfer swapchain images to right layout");

        // create depth buffer.
        if (_cp.depthStencilFormat != vk::Format::eUndefined) {
            _depthBuffer.reset(new Image(Image::ConstructParameters {{"swapchain depth buffer"}, gi}.setDepth(w, h, _cp.depthStencilFormat)));
            Barrier()
                .i(_depthBuffer->handle(), vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)
                .cmdWrite(c);
        }

        // initialize back buffer array.
        _backbuffers.resize(images.size());
        for (size_t i = 0; i < images.size(); ++i) {
            auto & bb = _backbuffers[i];
            bb.extent = vk::Extent2D {w, h};
            bb.format = swapchainCreateInfo.imageFormat;
            bb.image  = images[i];
            bb.view   = gi->device.createImageView(vk::ImageViewCreateInfo({}, bb.image, vk::ImageViewType::e2D, _cp.backbufferFormat)
                                                       .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}),
                                                   gi->allocator);
            setVkObjectName(gi->device, bb.image, format("back buffer image %zu", i));
            setVkObjectName(gi->device, bb.view, format("back buffer view %zu", i));
            // TODO: setup default layout.

            // create frame buffer
            auto fbcp = Framebuffer::ConstructParameters {{format("swapchain framebuffer %zu", i)}, gi}.addImageView(bb.view).setExtent(w, h).setRenderPass(
                *_renderPass);
            if (_depthBuffer) fbcp.addImageView(_depthBuffer->getView({}));
            bb.fb.reset(new Framebuffer(fbcp));

            // transfer backbuffers to right layout.
            Barrier()
                .i(bb.image, vk::AccessFlagBits::eNone, DESIRED_PRESENT_STATUS.access, vk::ImageLayout::eUndefined, DESIRED_PRESENT_STATUS.layout,
                   vk::ImageAspectFlagBits::eColor)
                .s(vk::PipelineStageFlagBits::eAllCommands, DESIRED_PRESENT_STATUS.stages)
                .cmdWrite(c);
        }

        // execute the command buffer to update image layout
        _graphicsQueue->submit({c});
        _graphicsQueue->wait();

        // initialize frame array.
        RVI_ASSERT(_backbuffers.size() > surfaceCaps.minImageCount);
        _frames.resize(std::max<size_t>(1u, _backbuffers.size() - surfaceCaps.minImageCount));
        for (size_t i = 0; i < _frames.size(); ++i) {
            auto & f            = _frames[i];
            f.imageAvailable    = gi->device.createSemaphore({}, gi->allocator);
            f.renderFinished    = gi->device.createSemaphore({}, gi->allocator);
            f.frameEndSemaphore = gi->device.createSemaphore({}, gi->allocator);
            setVkObjectName(gi->device, f.imageAvailable, format("image available semaphore %zu", i));
            setVkObjectName(gi->device, f.renderFinished, format("render finished semaphore %zu", i));
            setVkObjectName(gi->device, f.frameEndSemaphore, format("frame end semaphore %zu", i));
        }
    }

    void recreateHeadlessSwapchain() {
        clearSwapchain();
        auto gi = _cp.gi;

        // determine the swapchain size
        auto w = (uint32_t) _cp.width;
        auto h = (uint32_t) _cp.height;
        RVI_REQUIRE(w > 0 && h > 0, "Headless swapchain's width and height can't be zero.");

        // create a graphics command buffer to transfer swapchain images to the right layout.
        auto c = _graphicsQueue->begin("transfer swapchain images to right layout");

        // create depth buffer.
        if (_cp.depthStencilFormat != vk::Format::eUndefined) {
            _depthBuffer.reset(new Image(Image::ConstructParameters {{"swapchain depth buffer"}, gi}.setDepth(w, h, _cp.depthStencilFormat)));
            Barrier()
                .i(_depthBuffer->handle(), vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::ImageLayout::eUndefined,
                   vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)
                .cmdWrite(c);
        }

        // create back buffer and frame array.
        auto imageCount = _cp.maxFramesInFlight + 1;
        _backbuffers.resize(imageCount);
        _frames.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            // setup frame structure
            auto & bb           = _backbuffers[i];
            auto & f            = _frames[i];
            f.backbuffer        = &bb;
            f.imageIndex        = i;
            f.imageAvailable    = gi->device.createSemaphore({}, gi->allocator);
            f.renderFinished    = gi->device.createSemaphore({}, gi->allocator);
            f.frameEndSemaphore = gi->device.createSemaphore({}, gi->allocator);
            f.headlessImage.reset(new Image(Image::ConstructParameters {{"swapchain headless image"}, gi}
                                                .setFormat(_cp.backbufferFormat)
                                                .set2D(w, h)
                                                .addUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)));
            setVkObjectName(gi->device, f.imageAvailable, format("image available semaphore %u", i));
            setVkObjectName(gi->device, f.renderFinished, format("render finished semaphore %u", i));
            setVkObjectName(gi->device, f.frameEndSemaphore, format("frame end semaphore %u", i));

            // transfer the image into desired layout.
            Barrier()
                .i(f.headlessImage->handle(), vk::AccessFlagBits::eNone, DESIRED_PRESENT_STATUS.access, vk::ImageLayout::eUndefined,
                   DESIRED_PRESENT_STATUS.layout, vk::ImageAspectFlagBits::eColor)
                .s(vk::PipelineStageFlagBits::eAllCommands, DESIRED_PRESENT_STATUS.stages)
                .cmdWrite(c);

            // Store the image to back buffer structure
            bb.extent = vk::Extent2D {w, h};
            bb.image  = f.headlessImage->handle();

            // create back buffer view
            bb.view = gi->device.createImageView(vk::ImageViewCreateInfo({}, bb.image, vk::ImageViewType::e2D, _cp.backbufferFormat)
                                                     .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}),
                                                 gi->allocator);
            setVkObjectName(gi->device, bb.view, format("back buffer view %u", i));

            // create frame buffer
            auto fbcp = Framebuffer::ConstructParameters {{format("swapchain framebuffer %u", i)}, gi}.addImageView(bb.view).setExtent(w, h).setRenderPass(
                *_renderPass);
            if (_depthBuffer) fbcp.addImageView(_depthBuffer->getView({}));
            bb.fb.reset(new Framebuffer(fbcp));
        }

        // execute the command buffer to update image layout
        _graphicsQueue->submit({c});

        // do dummy submits to signal image available signals for all frames.
        for (auto & f : _frames) {
            auto cb = _graphicsQueue->begin("dummy submit to signal image available semaphore");
            _graphicsQueue->submit({cb, {}, {}, {f.imageAvailable}});
        }

        _graphicsQueue->wait();
    }

    void beginFrame() {
        auto & frame = _frames[_frameIndex % std::size(_frames)];
        frame.index  = _frameIndex;

        // wait for the frame to be available again.
        if (frame.frameEndCommands) {
            _graphicsQueue->wait(frame.frameEndCommands);
            frame.frameEndCommands = nullptr; // Clear the command buffer. So we only wait it once.
        }

        // Acquire the next available swapchain image. Only do this if we are not in headless mode.
        if (_handle) {
            frame.imageIndex = _cp.gi->device.acquireNextImageKHR(_handle, uint64_t(-1), frame.imageAvailable).value;
            RAPID_VULKAN_ASSERT(frame.imageIndex < _backbuffers.size());
            frame.backbuffer = &_backbuffers[frame.imageIndex];
        }
    }
};

// ---------------------------------------------------------------------------------------------------------------------
//
Swapchain::Swapchain(const ConstructParameters & cp): Root(cp) { _impl = new Impl(*this, cp); }
Swapchain::~Swapchain() {
    delete _impl;
    _impl = nullptr;
}
auto Swapchain::renderPass() const -> const RenderPass & { return _impl->renderPass(); }
void Swapchain::cmdBeginBuiltInRenderPass(vk::CommandBuffer cb, const BeginRenderPassParameters & bp) { return _impl->cmdBeginBuiltInRenderPass(cb, bp); }
void Swapchain::cmdEndBuiltInRenderPass(vk::CommandBuffer cb) { return _impl->cmdEndBuiltInRenderPass(cb); }
auto Swapchain::currentFrame() const -> const Swapchain::Frame & { return _impl->currentFrame(); }
void Swapchain::present(const PresentParameters & pp) { return _impl->present(pp); }

// *********************************************************************************************************************
// Device
// *********************************************************************************************************************

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
    RVI_LOGI("%s", ss.str().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------
//
static void printDeviceFeatures(vk::PhysicalDevice physical, const PhysicalDeviceFeatureList & enabled, bool verbose) {
    // retrieve physical device properties
    vk::PhysicalDeviceProperties properties = physical.getProperties();

    // retrieve supported feature list
    vk::PhysicalDeviceFeatures supported = physical.getFeatures();

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
    RVI_LOGI("%s", ss.str().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------
//
static void printDeviceExtensions(vk::PhysicalDevice physical, const std::vector<vk::ExtensionProperties> & available,
                                  const std::vector<const char *> & enabled, bool verbose) {
    // retrieve physical device properties
    auto properties = physical.getProperties();

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
    RVI_LOGI("%s", ss.str().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------
//
static void printAvailableQueues(vk::PhysicalDevice physical, const std::vector<vk::QueueFamilyProperties> & queues, bool) {
    auto properties = physical.getProperties();

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

    RVI_LOGI("%s", ss.str().c_str());
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
            RVI_LOGW("Optional feature %s is not supported by the current device.", a.first.c_str());
        }
    }
    return supported;
}

// ---------------------------------------------------------------------------------------------------------------------
//
Device::Device(const ConstructParameters & cp): _cp(cp) {
    // check API version
    if (0 == _cp.apiVersion) _cp.apiVersion = vk::enumerateInstanceVersion();
    _gi.apiVersion = _cp.apiVersion;

    // check instance pointer
    RVI_REQUIRE(cp.instance);
    _gi.instance = cp.instance;

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

    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures(true);
    deviceFeatures.addFeature(extendedDynamicStateFeatures);

    // some extensions are always enabled by default
    askedDeviceExtensions[VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME] = true;
    askedDeviceExtensions[VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME]    = true;

    // enable swapchian extension regardless to support VK_IMAGE_LAYOUT_PRESENT_SRC.
    askedDeviceExtensions[VK_KHR_SWAPCHAIN_EXTENSION_NAME] = true;

    // #if PH_ANDROID == 0
    //     if (isRenderDocPresent()) {                                                       // only add this when renderdoc is available
    //         askedDeviceExtensions[VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME] = true; // add this to allow debugging on compute shaders
    //     }
    // #endif

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

    //     // initialize a memory allocator for Vulkan images
    //     if (cp.enableVmaAllocator) {
    //         VmaAllocatorCreateInfo ai {};
    //         ai.vulkanApiVersion = cp.apiVersion;
    //         ai.physicalDevice   = _gi.physical;
    //         ai.device           = _gi.device;
    //         ai.instance         = _gi.instance;

    // #if 0 // uncomment this section to enable vma allocation recording
    //         VmaRecordSettings vmaRecordSettings;
    //         vmaRecordSettings.pFilePath = "vmaReplay.csv";
    //         vmaRecordSettings.flags = VMA_RECORD_FLUSH_AFTER_CALL_BIT;
    //         ai.pRecordSettings = &vmaRecordSettings;
    // #endif
    //         if (askedDeviceExtensions.find(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) != askedDeviceExtensions.end()) {
    //             RVI_LOGI("Enable VMA allocator with buffer device address.");
    //             ai.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    //         }
    //         RVI_VK_REQUIRE(vmaCreateAllocator(&ai, &_gi.vmaAllocator));
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
        auto q     = new CommandQueue({{name}, &_gi, i, 0});
        _queues[i] = q;

        // classify all queues
        if (!_graphics && f.queueFlags & vk::QueueFlagBits::eGraphics) {
            _graphics               = q;
            _gi.graphicsQueueFamily = q->family();
        }

        if (!_compute && !(f.queueFlags & vk::QueueFlagBits::eGraphics) && (f.queueFlags & vk::QueueFlagBits::eCompute)) _compute = q;

        if (!_transfer && !(f.queueFlags & vk::QueueFlagBits::eGraphics) && !(f.queueFlags & vk::QueueFlagBits::eCompute) &&
            (f.queueFlags & vk::QueueFlagBits::eTransfer))
            _transfer = q;

        if (cp.surface && !_present) {
            auto supportPresenting = _gi.physical.getSurfaceSupportKHR(i, cp.surface);
            if (supportPresenting) _present = q;
        }
    }

    RVI_LOGI("Vulkan device initialized.");
}

// ---------------------------------------------------------------------------------------------------------------------
//
Device::~Device() {
    for (auto q : _queues) delete q;
    _queues.clear();
#if RAPID_VULKAN_ENABLE_VMA
    if (_gi.vmaAllocator) vmaDestroyAllocator(_gi.vmaAllocator), _gi.vmaAllocator = nullptr;
#endif
    if (_gi.device) {
        RVI_LOGI("[Device] destroying device...");
        _gi.device.destroy(_gi.allocator);
        _gi.device = nullptr;
        RVI_LOGI("[Device] device destroyed");
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

        auto processSupportedExtension = [&](const vk::ExtensionProperties & available_) {
            // Add supported ones to v.instanceExtensions list. Then remove it from extensions_ list. So we won't check
            // the same extension twice.
            for (auto asked = extensions_.begin(); asked != extensions_.end();) {
                if (0 == strcmp(asked->first, available_.extensionName)) {
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
                    RVI_LOGW("Optional VK layer %s is not supported.", l.first);
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
                RVI_LOGW("Optional VK extension %s is not supported.", asked.first);
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

// ---------------------------------------------------------------------------------------------------------------------
//
static VkBool32 VKAPI_PTR staticDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location,
                                              int32_t messageCode, const char * prefix, const char * message, void * userData) {
    auto reportVkError = [&]() {
        // if (objectType == vk::DebugReportObjectTypeEXT::eDevice && _lost) {
        //     // Ignore validation errors on lost device, to avoid spamming log with useless messages.
        //     return VK_FALSE;
        // }
        (void) objectType;
        (void) object;
        (void) location;
        (void) messageCode;
        (void) prefix;

        Instance *   instance = reinterpret_cast<Instance *>(userData);
        const auto & cp       = instance->cp();

        auto ss = std::stringstream();
        ss << "[Vulkan] " << prefix << " : " << message;
        // if (v >= LOG_ON_VK_ERROR_WITH_CALL_STACK) { ss << std::endl << backtrace(false); }
        auto str = ss.str();
        RVI_LOGE("%s", str.data());
        if (cp.validation == Instance::THROW_ON_VK_ERROR) {
            RVI_THROW("%s", str.data());
        } else if (cp.validation == Instance::BREAK_ON_VK_ERROR) {
#ifdef _WIN32
            ::DebugBreak();
#elif __ANDROID__
            __builtin_trap();
#else
            asm("int $3");
#endif
        }

        return VK_FALSE;
    };

    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) reportVkError();

    // treat warning as error.
    // if (flags & vk::DebugReportFlagBitsEXT::eWarning) reportVkError();

    // if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    //     RVI_LOGW("[Vulkan] %s : %s", prefix, message);
    // }

    // if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
    //     RVI_LOGI("[Vulkan] %s : %s", prefix, message);
    // }

    // if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
    //     RVI_LOGI("[Vulkan] %s : %s", prefix, message);
    // }

    return VK_FALSE;
}

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

    // Determine API version
    if (0 == _cp.apiVersion)
        _cp.apiVersion = instanceInfo.version;
    else if (_cp.apiVersion > instanceInfo.version) {
        RVI_LOGW("Requested version %d is higher than the supported version %d. The instance will be created with %d instead.", _cp.apiVersion,
                 instanceInfo.version, instanceInfo.version);
        _cp.apiVersion = instanceInfo.version;
    }

    std::map<const char *, bool> layers;
    for (const auto & l : cp.layers) layers[l.first.c_str()] = l.second;
    if (cp.validation) {
        // add validation layer as an "optional" layer
        layers.insert({"VK_LAYER_KHRONOS_validation", false});
    }

    // setup extension list
    std::map<const char *, bool> instanceExtensions {
        {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, true},
        {VK_KHR_SURFACE_EXTENSION_NAME, true},
#ifdef _WIN32
        {"VK_KHR_win32_surface", false},
#elif defined(__ANDROID__)
        {"VK_KHR_android_surface", false},
#elif defined(__linux__)
        {"VK_KHR_xcb_surface", false},
        {"VK_KHR_xlib_surface", false},
        {"VK_KHR_wayland_surface", false},
#else // macOS
        {"VK_MVK_macos_surface", false},
        {"VK_EXT_metal_surface", false},
#endif
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
    RVI_LOGI("%s", instanceCreationPrompt.str().c_str());

    // turn off validation, if validation layer is not present
    if (cp.validation) {
        if (std::find_if(supported.layers.begin(), supported.layers.end(), [](const char * l) { return strcmp(l, "VK_LAYER_KHRONOS_validation") == 0; }) ==
            supported.layers.end()) {
            RVI_LOGW("Validation layer is not supported. Validation will be disabled.");
            cp.validation = VALIDATION_DISABLED;
        }
    }

    // create VK instance
    // TODO: check against available version.
    auto appInfo = vk::ApplicationInfo().setApiVersion(_cp.apiVersion);
    auto ici     = vk::InstanceCreateInfo()
                   .setPNext(buildStructureChain(_cp.instanceCreateInfo.begin(), _cp.instanceCreateInfo.end()))
                   .setPApplicationInfo(&appInfo)
                   .setPEnabledLayerNames(supported.layers)
                   .setPEnabledExtensionNames(supported.instanceExtensions);
    try {
        _instance = vk::createInstance(ici);
    } catch (vk::SystemError & e) {
        // TODO: print instance information
        RVI_LOGE("Failed to create Vulkan instance: %s", e.what());
        throw;
    }

    // Print instance information
    if (cp.printVkInfo) {
        auto message = instanceInfo.print(ici, Device::VERBOSE == cp.printVkInfo);
        RVI_LOGI("%s", message.data());
    }

#if RAPID_VULKAN_ENABLE_LOADER
    // load all function pointers.
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);
#endif

    // setup debug callback
    if (cp.validation) {
        auto debugci = vk::DebugReportCallbackCreateInfoEXT()
                           .setFlags(vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning)
                           .setPfnCallback(staticDebugCallback)
                           .setPUserData(this);
        _debugReport = _instance.createDebugReportCallbackEXT(debugci);
    }

    RVI_LOGI("Vulkan instance initialized.");
}

// ---------------------------------------------------------------------------------------------------------------------
//
Instance::~Instance() {
    if (_debugReport) {
        _instance.destroyDebugReportCallbackEXT(_debugReport);
        _debugReport = VK_NULL_HANDLE;
    }
    if (_instance) _instance.destroy(), _instance = VK_NULL_HANDLE;
    RVI_LOGI("Vulkan instance destroyed.");
}

} // namespace RAPID_VULKAN_NAMESPACE
