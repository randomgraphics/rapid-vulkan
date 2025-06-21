/*
    This is a slightly more complex sample than the triangle sample. It demonstrates the basic usage of the Drawable class.
*/
#include "../rv.h"
#include <iostream>
#include <cmath>
#include <thread>

namespace drawable {

#include "shader/pipeline.vert.spv.h"
#include "shader/pipeline.frag.spv.h"

struct GLFWInit {
    vk::Instance inst;
    GLFWwindow * window  = nullptr;
    VkSurfaceKHR surface = nullptr;
    GLFWInit(bool headless, vk::Instance instance, uint32_t w, uint32_t h, const char * title): inst(instance) {
        if (headless) return; // do nothing in headless mode.
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow((int) w, (int) h, title, nullptr, nullptr);
        RVI_REQUIRE(window, "Failed to create GLFW window.");
        glfwCreateWindowSurface(instance, window, nullptr, &surface);
        RVI_REQUIRE(surface, "Failed to create surface.");
    }
    ~GLFWInit() {
        if (surface) inst.destroySurfaceKHR(surface), surface = VK_NULL_HANDLE;
        if (window) glfwDestroyWindow(window), window = nullptr;
        glfwTerminate();
    }
    void show() {
        if (window) glfwShowWindow(window);
    }
    bool processEvents() const {
        if (!window) return false;
        glfwPollEvents();
        if (glfwWindowShouldClose(window)) return false;
        while (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            glfwPollEvents();
        }
        return true;
    }
};

struct Options {
    vk::Instance                    inst      = VK_NULL_HANDLE;
    uint32_t                        headless  = 0; // Set to non-zero to enable headless mode. The value is number of frames to render.
    rapid_vulkan::Device::Verbosity verbosity = rapid_vulkan::Device::BRIEF;
};

void entry(const Options & options) {
    // Standard boilerplate of creating instance, device, swapchain, etc. It is basically the same as triangle.cpp.
    using namespace rapid_vulkan;
    auto                      instance = options.inst;
    std::unique_ptr<Instance> instancePtr;
    if (!instance) {
        instancePtr = std::make_unique<Instance>(Instance::ConstructParameters {}.setValidation(Instance::BREAK_ON_VK_ERROR).setBacktrace(backtrace));
        instance    = instancePtr->handle();
    }
    auto device = Device(Device::ConstructParameters {instance}.setPrintVkInfo(options.verbosity));
    auto gi     = device.gi();
    auto q      = CommandQueue({{"main"}, gi, device.graphics()->family(), device.graphics()->index()});
    auto w      = uint32_t(1280);
    auto h      = uint32_t(720);
    auto glfw   = GLFWInit(options.headless, instance, w, h, "pipeline-args");
    auto sw     = Swapchain(Swapchain::ConstructParameters {{"swapchain"}}
                                .setSurface(options.headless ? nullptr : glfw.surface)
                                .setDevice(device)
                                .setDimensions(options.headless ? w : 0, options.headless ? h : 0));
    auto vs     = Shader(Shader::ConstructParameters {{"vs"}}.setGi(gi).setSpirv(pipeline_vert));
    auto fs     = Shader(Shader::ConstructParameters {{"fs"}, gi}.setSpirv(pipeline_frag));
    auto p      = Ref<GraphicsPipeline>::make(GraphicsPipeline::ConstructParameters {}
                                                  .setRenderPass(sw.renderPass())
                                                  .setVS(&vs)
                                                  .setFS(&fs)
                                                  .dynamicScissor()
                                                  .dynamicViewport()
                                                  .addVertexAttribute(0, 0, vk::Format::eR32G32Sfloat)
                                                  .addVertexBuffer(2 * sizeof(float)));

    // This part is what this sample is about. We create some buffers and bind them to the drawable.
    auto u0 = Ref(new Buffer(Buffer::ConstructParameters {{"ub0"}, gi}.setUniform().setSize(sizeof(float) * 2)));
    auto u1 = Ref(new Buffer(Buffer::ConstructParameters {{"ub1"}, gi}.setUniform().setSize(sizeof(float) * 3)));
    auto bc = Buffer::SetContentParameters {}.setQueue(*device.graphics());
    auto vb = Ref(new Buffer(Buffer::ConstructParameters {{"vb"}, gi}.setVertex().setSize(sizeof(float) * 2 * 3)));
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
    glfw.show();
    for (;;) {
        if (!options.headless && !glfw.processEvents()) break;
        auto frame = sw.beginFrame();
        if (frame) {
            // Standard boilerplate of rendering a frame. It is basically the same as triangle.cpp.
            if (options.headless) {
                if (frame->index > options.headless) break; // render required number of frames in headless mode, then quit.
                std::cout << "Frame " << frame->index << std::endl;
            }
            // Animate the triangle. Note that this is not the most efficient way to animate things, since it serializes
            // CPU and GPU. But it's simple and it is not the focus of this sample.
            auto elapsed = (float) frame->index / 60.0f;
            u0->setContent(bc.setData<float>({(float) std::sin(elapsed) * .25f, (float) std::cos(elapsed) * .25f}));
            u1->setContent(bc.setData<float>({(float) std::sin(elapsed) * .5f + .5f, (float) std::cos(elapsed) * .5f + .5f, 1.f}));

            // acquire a command buffer
            auto c = q.begin("pipeline");

            // begin the render pass
            sw.cmdBeginBuiltInRenderPass(c, Swapchain::BeginRenderPassParameters {}.setClearColorF({0.0f, 1.0f, 0.0f, 1.0f})); // clear to green

            // compile the drawable to generate the draw pack. Draw pack is a renderable and immutable representation of
            // the current state of the drawable.
            auto dp = dr->compile();

            // enqueue the draw pack to command buffer.
            c.render(dp);

            // end render pass
            sw.cmdEndBuiltInRenderPass(c);

            // submit the command buffer
            q.submit({c, {}, {frame->imageAvailable}, {frame->renderFinished}});
        }

        // end of the frame.
        sw.present({});
    }
}

} // namespace drawable

#ifndef UNIT_TEST
int main() { drawable::entry({}); }
#endif