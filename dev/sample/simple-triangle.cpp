#include "../rv.h"
#include "shader/simple-triangle.vert.spv.h"
#include "shader/simple-triangle.frag.spv.h"

struct GLFWInit {
    GLFWInit() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    ~GLFWInit() { glfwTerminate(); }
};

int main() {
    using namespace rapid_vulkan;
    auto w        = uint32_t(1280);
    auto h        = uint32_t(720);
    auto glfw     = GLFWInit();
    auto window   = glfwCreateWindow(w, h, "simple-triangle", nullptr, nullptr);
    auto instance = Instance(Instance::ConstructParameters {}.setValidation(Instance::BREAK_ON_VK_ERROR));
    auto surface  = createGLFWSurface(instance, window);
    auto device   = Device(instance.dcp().setSurface(surface.get()));
    auto gi       = device.gi();
    auto vs       = Shader(Shader::ConstructParameters {{"simple-triangle-vs"}}.setGi(gi).setSpirv(simple_triangle_vert));
    auto fs       = Shader(Shader::ConstructParameters {{"simple-triangle-fs"}, gi}.setSpirv(simple_triangle_frag));
    auto q        = CommandQueue({{"main"}, gi, device.graphics()->family(), device.graphics()->index()});
    auto sw       = Swapchain(Swapchain::ConstructParameters {{"simple-triangle"}}.setDevice(device));

    // create the graphics pipeline
    auto gcp = GraphicsPipeline::ConstructParameters {{"simple-triangle"}}.setRenderPass(sw.renderPass()).setVS(&vs).setFS(&fs);
    gcp.rast.setCullMode(vk::CullModeFlagBits::eNone);
#if 1
    // Dynamic viewport and scissor. When window surface is resized, the viewport and scissor will be updated automatically w/o recreating the pipeline.
    gcp.dynamicScissor().dynamicViewport();
#else
    // When using static viewport and scissor, the viewport won't change along with the window surface.g st
    gcp.viewports.push_back(vk::Viewport(0.f, 0.f, (float) w, (float) h, 0.f, 1.f));
    gcp.scissors.push_back(vk::Rect2D({0, 0}, {w, h}));
#endif
    auto p = GraphicsPipeline(gcp);

    // Endless render loop for this simple sample.
    glfwShowWindow(window);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        auto & frame = sw.currentFrame();
        auto   c     = q.begin("simple-triangle");
        sw.cmdBeginBuiltInRenderPass(c, Swapchain::BeginRenderPassParameters {}.setColorF(0.0f, 1.0f, 0.0f, 1.0f)); // clear to green
        p.cmdDraw(c, GraphicsPipeline::DrawParameters {}.setNonIndexed(3));                                         // then draw a blue triangle.
        sw.cmdEndBuiltInRenderPass(c);
        q.submit({c, {}, {frame.imageAvailable}, {frame.renderFinished}});
        sw.present({});
    };

    glfwDestroyWindow(window);
}
