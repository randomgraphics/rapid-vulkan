/*
    This is a slightly more complex sample than the triangle sample. It demonstrates the basic usage of the Drawable class.
*/
#include "common.h"

#include "../sample/shader/pipeline.vert.spv.h"
#include "../sample/shader/pipeline.frag.spv.h"

int main() {
    using namespace rapid_vulkan;

    RenderBase base(false, 1280, 720, "streaming");
    auto gi = base.device->gi();
    
    // Standard boilerplate of creating instance, device, swapchain, etc. It is basically the same as triangle.cpp.
    auto vs     = Shader(Shader::ConstructParameters {{"vs"}}.setGi(gi).setSpirv(pipeline_vert));
    auto fs     = Shader(Shader::ConstructParameters {{"fs"}, gi}.setSpirv(pipeline_frag));
    auto p      = Ref<GraphicsPipeline>::make(GraphicsPipeline::ConstructParameters {}
                                                  .setRenderPass(base.swapchain->renderPass())
                                                  .setVS(&vs)
                                                  .setFS(&fs)
                                                  .dynamicScissor()
                                                  .dynamicViewport()
                                                  .addVertexAttribute(0, 0, vk::Format::eR32G32Sfloat)
                                                  .addVertexBuffer(2 * sizeof(float)));

    // This part is what this sample is about. We create some buffers and bind them to the drawable.
    auto u0 = Ref(new Buffer(Buffer::ConstructParameters {{"ub0"}, gi}.setUniform().setSize(sizeof(float) * 2)));
    auto u1 = Ref(new Buffer(Buffer::ConstructParameters {{"ub1"}, gi}.setUniform().setSize(sizeof(float) * 3)));
    auto vb = Ref(new Buffer(Buffer::ConstructParameters {{"vb"}, gi}.setVertex().setSize(sizeof(float) * 2 * 3)));
    auto bc = Buffer::SetContentParameters {}.setQueue(*base.device->graphics());
    vb->setContent(bc.setData<float>({-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f}));

    // create a drawable object using the pipeline object.
    auto dr = Ref(new Drawable({{}, p}));

    // Bind uniform buffer u0 to set 0, index 0
    dr->b({0, 0}, {{u0}});

    // Bind uniform buffer u1 to set 0, index 1
    dr->b({0, 1}, {{u1}});

    // Bind vb as the vertex buffer.
    dr->v({{vb}});

    // setup draw parameters of the drawable to have 3 non-indexed vertices.
    dr->draw(GraphicsPipeline::DrawParameters {}.setNonIndexed(3));

    // show the window and begin the rendering loop.
    base.show();
    for (;;) {
        if (!base.processEvents()) break;
        auto frame = base.swapchain->beginFrame();
        if (frame) {
            // Animate the triangle. Note that this is not the most efficient way to animate things, since it serializes
            // CPU and GPU. But it's simple and it is not the focus of this sample.
            auto elapsed = (float) frame->index / 60.0f;
            u0->setContent(bc.setData<float>({(float) std::sin(elapsed) * .25f, (float) std::cos(elapsed) * .25f}));
            u1->setContent(bc.setData<float>({(float) std::sin(elapsed) * .5f + .5f, (float) std::cos(elapsed) * .5f + .5f, 1.f}));

            // acquire a command buffer
            auto c = base.commandQueue->begin("pipeline");

            // begin the render pass
            base.swapchain->cmdBeginBuiltInRenderPass(c, Swapchain::BeginRenderPassParameters {}.setClearColorF({0.0f, 1.0f, 0.0f, 1.0f})); // clear to green

            // compile the drawable to generate the draw pack. Draw pack is a renderable and immutable representation of
            // the current state of the drawable.
            auto dp = dr->compile();

            // enqueue the draw pack to command buffer.
            c.render(dp);

            // end render pass
            base.swapchain->cmdEndBuiltInRenderPass(c);

            // submit the command buffer
            base.commandQueue->submit({c, {}, {frame->imageAvailable}, {frame->renderFinished}});
        }

        // end of the frame.
        base.swapchain->present({});
    }
    base.device->waitIdle();
}
