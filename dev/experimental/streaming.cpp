/*
    This is a slightly more complex sample than the triangle sample. It demonstrates the basic usage of the Drawable class.
*/
#include "common.h"

#include "../sample/shader/pipeline.vert.spv.h"
#include "../sample/shader/pipeline.frag.spv.h"

int main() {
    using namespace rapid_vulkan;

    RenderBase base(false, 1280, 720, "streaming");
    auto       gi = base.device->gi();

    // setup vertex buffer with 3 vertices of the triangle.
    auto vb = Ref(new Buffer(Buffer::ConstructParameters {{"vb"}, gi}.setVertex().setSize(sizeof(float) * 2 * 3)));
    auto bc = Buffer::SetContentParameters {}.setQueue(*base.device->graphics());
    vb->setContent(bc.setData<float>({-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f}));

    // create a drawable that draws a triangle to screen.
    auto vs = Shader(Shader::ConstructParameters {{"vs"}}.setGi(gi).setSpirv(pipeline_vert));
    auto fs = Shader(Shader::ConstructParameters {{"fs"}, gi}.setSpirv(pipeline_frag));
    auto p  = Ref<GraphicsPipeline>::make(GraphicsPipeline::ConstructParameters {}
                                              .setRenderPass(base.swapchain->renderPass())
                                              .setVS(&vs)
                                              .setFS(&fs)
                                              .dynamicScissor()
                                              .dynamicViewport()
                                              .addVertexAttribute(0, 0, vk::Format::eR32G32Sfloat)
                                              .addVertexBuffer(2 * sizeof(float)));
    auto u0 = Ref(new Buffer(Buffer::ConstructParameters {{"ub0"}, gi}.setUniform().setSize(sizeof(float) * 2))); // vertex offset
    auto u1 = Ref(new Buffer(Buffer::ConstructParameters {{"ub1"}, gi}.setUniform().setSize(sizeof(float) * 3))); // color
    auto dr = Ref(new Drawable({{}, p}));
    dr->b({0, 0}, {{u0}});                                          // bind vertex offset to set 0, index 0
    dr->b({0, 1}, {{u1}});                                          // bind color to set 0, index 1
    dr->v({{vb}});                                                  // bind vertex buffer
    dr->draw(GraphicsPipeline::DrawParameters {}.setNonIndexed(3)); // draw 3 vertices

    // show the window and begin the rendering loop.
    base.show();
    for (;;) {
        if (!base.processEvents()) break;
        auto frame = base.swapchain->beginFrame();
        if (frame) {
            // animate the scene
            auto elapsed = (float) frame->index / 60.0f;
            auto offsetX = (float) std::sin(elapsed) * .25f;
            auto offsetY = (float) std::cos(elapsed) * .25f;
            auto colorR  = (float) std::sin(elapsed) * .5f + .5f;
            auto colorG  = (float) std::cos(elapsed) * .5f + .5f;

            // allocate a one time staging buffer to update the uniform buffer.
            auto staging = Ref<Buffer>::make(Buffer::ConstructParameters {{"staging"}, gi}.setStaging().setSize(sizeof(float) * 2 + sizeof(float) * 3));
            {
                auto m    = Buffer::Map<float>(*staging, 0, sizeof(float) * 2 + sizeof(float) * 3);
                m.data[0] = offsetX;
                m.data[1] = offsetY;
                m.data[2] = colorR;
                m.data[3] = colorG;
                m.data[4] = 1.0f;
            }

            // acquire a command buffer
            auto c = base.commandQueue->begin("pipeline");

            // copy the staging buffer to the uniform buffer.
            staging->cmdCopyTo({c, u0->handle(), u0->desc().size, 0, 0, sizeof(float) * 2});
            staging->cmdCopyTo({c, u1->handle(), u0->desc().size, 0, sizeof(float) * 2, sizeof(float) * 3});

            // Make sure the staging buffer is released after the command buffer is finished (meaning copy is done).
            c.onFinished(
                [staging, index = frame->index](bool) mutable {
                    staging.reset();
                    RVI_LOGI("staging buffer released for frame %zu", index);
                },
                "release staging buffer");

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
