#include "../3rd-party/catch2/catch.hpp"
#include "test-instance.h"
#include "shader/full-screen.vert.spv.h"
#include "shader/texture-array.frag.spv.h"
#include "rdc.h"

using namespace rapid_vulkan;

Ref<Pipeline> createPipeline(std::string name, vk::RenderPass renderPass, vk::ArrayProxy<const unsigned char> vs_, vk::ArrayProxy<const unsigned char> fs_) {
    auto device = TestVulkanInstance::device.get();
    auto gi     = device->gi();
    auto vs     = Shader(Shader::ConstructParameters {{name + "-vs"}, gi}.setSpirv(vs_));
    auto fs     = Shader(Shader::ConstructParameters {{name + "-fs"}, gi}.setSpirv(fs_));
    auto gcp    = GraphicsPipeline::ConstructParameters {{name}};
    gcp.setRenderPass(renderPass).setVS(&vs).setFS(&fs).dynamicViewport().dynamicScissor();
    return new GraphicsPipeline(gcp);
}

struct TextureArrayUniform {
    float u, v;
    int   index;
};

TEST_CASE("texture-array", "[perf]") {
    auto device = TestVulkanInstance::device.get();
    auto gi     = device->gi();
    auto w      = uint32_t(128);
    auto h      = uint32_t(72);
    auto sw     = Swapchain(Swapchain::ConstructParameters {{"vertex-buffer-test"}}.setDevice(*device).setDimensions(w, h));
    auto q      = device->graphics();

    // create texture array
    auto N = 1000u;
    auto t = Image(Image::ConstructParameters {{"texture-array"}, gi}.set2D(2, 2).setFormat(vk::Format::eR8G8B8A8Unorm));
    auto s = Sampler(Sampler::ConstructParameters {{"texture-array"}, gi}.setLinear());
    auto a = std::vector<ImageSampler>(N);
    for (uint32_t i = 0; i < N; ++i) {
        ImageSampler & is = a[i];
        is.imageView      = t.getView({});
        is.imageLayout    = vk::ImageLayout::eShaderReadOnlyOptimal;
        is.sampler        = s.handle();
    }

    // create drawable
    auto p = createPipeline("texture-array", sw.renderPass(), full_screen_vert, texture_array_frag);
    auto d = Drawable({{"texture-array"}, p});
    auto u = Buffer(Buffer::ConstructParameters {{"texture-array"}, gi}.setSize(sizeof(TextureArrayUniform)).setUniform());
    d.b({0, 0}, {{u}});
    d.t({0, 1}, a);

    auto c = q->begin("texture-array");
    Barrier()
        .i(t.handle(), vk::AccessFlagBits::eNone, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal,
           vk::ImageAspectFlagBits::eColor)
        .cmdWrite(c);
    sw.cmdBeginBuiltInRenderPass(c.handle(), {});
    {
        ScopedTimer timer("render-drawbles");
        for (size_t i = 0; i < 100000; ++i) { c.render(*d.compile()); }
    }
    sw.cmdEndBuiltInRenderPass(c.handle());
    q->submit({c}).wait();
}
