# rapid-vulkan
rapid-vulkan is a lightweight wrappers and utilities for rapid creation of Vulkan app. It is designed in a header-only manner that can be easiy integrated into any Vulkan based applications.

The library is compiled against C++17 standard. It depends on the C++ version of the Vulkan header (vulkan.hpp) coming with Vulkan SDK, and one additional 3rd-party library: **spirv-reflect**, which is already included in the repository.

It is currently developed and tested on both Windows 11 and Ubuntu 22.04.

The library is aiming for decreasing the code you have to write yourself to use Vulkan. The following is an example that loads and executes a Vulkan compute shader using rapid-vulkan:

```c++
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan/rapid-vulkan.h>
int main() {
  using namespace rapid_vulkan;
  auto instance = Instance({});
  auto device   = Device(instance.dcp()};
  auto spv      = loadSPRIV(...); // load spirv shader binary into std::vector container.
  auto cs       = Shader(Shader::ConstructParameters{{"my shader"}, device->gi}.setSpriv(spv));
  auto pipeline = ComputePipeline({{"my pipline"}, &cs});
  auto queue    = device->compute();
  auto commands = q.begin("my command buffer");
  pipeline.cmdDispatch(commands, {1, 1, 1});
  queue.submit({commands});
  queue.wait();
  return 0;
}
```

# Integration
The library aims for easy integration with any Vulkan project. Everything you need is included in [inc](inc) folder.

Here are how you integrate it with your project:

1. Copy everything in [inc](inc) folder into your project's source folder.
2. Include [rapid-vulkan.h](inc/rapid-vulkan/rapid-vulkan.h) anywhere you like.
3. In **one and only one** of your source files, include [rapid-vulkan.h](inc/rapid-vulkan/rapid-vulkan.h) with RAPID_VULKAN_IMPLEMENTATION macro defined in front of it. Then you are good to go:

```c
// in your header:
#include <rapid-vulkan/rapid-vulkan.h>

// in one of your source files:
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan/rapid-vulkan.h>
```
Please checkout apps in [sample](dev/sample) folder for how to use rapid-vulkan to quickly create Vulkan applications.

# Compile Time Configurations

All macros started with RAPID_VULKAN_ (like RAPID_VULKAN_ASSERT) are compile time configurations that you can set to customize rapid-vulkan's behavior. Usage of those macros are explained in the header at where they are defined.

# Build Sample & Test Apps
Again, rapid-vulkan is a header-only library. No build is required use it. The following build steps are only to build test and sample apps.

On Linux, just run [bootstrap.sh](dev/env/bootstrap.sh) to install all dependencies. Or you can use the docker container coming with the library. Just run `dev/docker/desktop/run.sh` to launch the docker environment.

On Windows, you'll need to manually install Vulkan SDK, as well as cmake, git and python 3.8+. Of course, you'll also need to have latest version of Visual Studio.

After that, run [env.sh](env.sh)/[env.cmd](env.cmd) to launch the dev console. Then type `b d` to build debug variant, `b p` to build profile variant or `b r` to build release variant. Type `b --help` for detail help.

After everything is built. You can use `cit` command to launch the check-in-test suite to verify the library.

# Hello World
```c++
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan/rapid-vulkan.h>
int main() {
    auto instance = rapid_vulkan::Instance({});
    auto device   = rapid_vulkan::Device(instance.dcp());
    return 0;
}
```
This is the simplist form of an app created out of the rapid-vulkan library. It creates a Vulkan instance using default options. Then creates a Vulkan device out of that instance.

# Pipeline
VkPipeline sits at the center of Vulkan architecture that defines how GPU pipeline should be configured to render the scene. It is powerful but tediours to use. You'll have to create and manage an whole series of supporting objects, such as pipeline layout, descriptor set layout, descriptor set, descriptor pool to use it. To simplify this task, the rapid-vulkan library wraps all of them into 3 easy to use classes: **Pipeline**, **PipelineLayout** and **ArgumentPack**

Before going into details, here is an example of using these 3 classes to issue a draw command:

```c++
    auto rp  = getYourRenderPass(...);
    auto vs  = Shader(...);
    auto fs  = Shader(...);
    auto gcp = GraphicsPipeline::ConstructParameters {}.setRenderPass(rp).setVS(&vs).setFS(&fs);
    // TODO: set more pipeline parameters here.
    auto p = GraphicsPipeline(gcp);

```

**Pipeline** class is basically a wrapper of VkPipeline object. It has 2 sub classes for compute and graphcis pipeline. It has some utility methods that makes constructing a pipeline object a bit less verbose.

**PipelineLayout**

# License
The library is released under MIT license. See [LICENSE](LICENSE) file for details.
