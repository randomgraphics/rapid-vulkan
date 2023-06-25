#include "../3rd-party/catch2/catch.hpp"
#include "test-instance.h"
#include "shader/full-screen.vert.spv.h"
#include "shader/texture-array.frag.spv.h"
#include "rdc.h"

using namespace rapid_vulkan;

Ref<Pipeline> createPipeline(std::string name, vk::RenderPass renderPass, vk::ArrayProxy<const unsigned char> vs, vk::ArrayProxy<const unsigned char> fs) {
    auto device = TestVulkanInstance::device.get();
    auto gi     = device->gi();
    auto vs     = Shader(Shader::ConstructParameters {{name + "-vs"}, gi}.setSpirv(vs));
    auto fs     = Shader(Shader::ConstructParameters {{name + "-fs"}, gi}.setSpirv(fs));
    auto gcp    = GraphicsPipeline::ConstructParameters {{name}};
    gcp.setRenderPass(renderPass).setVS(&vs).setFS(&fs);
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
    auto p      = createPipeline("texture-array", sw.renderPass(), full_screen_vert, texture_array_frag);
    auto u      = Buffer(Buffer::ConstructParameters {{"texture-array"}, gi}.setSize(sizeof(TextureArrayUniform)).setUniform());
    auto d      = Drawable({{"texture-array"}}, p);

    // create texture array
    auto t = Image(Image::ConstructParameters {{"texture-array"}, gi}
                       .set2D(2, 2)
                       .setFormat(vk::Format::eR8G8B8A8Unorm)
                       .setInitialLayout(vk::ImageLayout::eShaderReadOnlyOptimal));
    auto s = Sampler(Sampler::ConstructParameters {{"texture-array"}, gi}.setLinear());
    auto a = std::vector<ImageSampler>(1024);
    for (uint32_t i = 0; i < 1024; ++i) {
        ImageSampler & is = a[i];
        is.imageView      = {t};
        is.imageLayout    = vk::ImageLayout::eShaderReadOnlyOptimal;
        is.sampler        = s;
    }

    d.b({0, 0}, {b});
    d.t({0, 1}, {t, s});

    auto c = q.begin("texture-array");
    sw.cmdBeginBuiltInRenderPass();
    for(size_t i = 0; i < 1000; ++i) {
        c.enqueue(d.compile());
    }
    sw.cmdEndBuiltInRenderPass(c);
    q.submit({c}).wait();
}
