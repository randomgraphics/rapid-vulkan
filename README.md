# rapid-vulkan
rapid-vulkan is a lightweight wrappers and utilities for rapid creation of Vulkan app.

The library depends on the C++ version of the Vulkan header (vulkan.hpp) coming with Vulkan SDK, and one additional 3rd-party library: **spirv-reflect**, which is already included in the repository.

It is currently developed and tested on both Windows and Ubuntu.

# Integration
The library is aiming for easy integration with any Vulkan project. Everything you need is included in [inc](inc)folder.

Here are the step by step instructions to integrate it with your project:

1. Copy everything in [inc](inc) folder into your project's source folder.
2. Include [rapid-vulkan.h](inc/rapid-vulkan/rapid-vulkan.h) anywhere you need rapid-vulkan functionality.
3. In **one and only one** of your source files, include [rapid-vulkan.h](inc/rapid-vulkan/rapid-vulkan.h) with RAPID_VULKAN_IMPLEMENTATION macro defined in front of it. Then you are good to go:

```c
// in your header:
#include <rapid-vulkan/rapid-vulkan.h>

// in one of your source files:
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan/rapid-vulkan.h>
```

# Compile Time Configurations

All macros started with RAPID_VULKAN_ (like RAPID_VULKAN_ASSERT) are compile time configurations that you can set to customize rapid-vulkan's behavior. All macros are explained in the header at where the macro is defined.

# Usage
Please checkout apps in [sample](dev/sample) folder for how to use rapid-vulkan to quickly create Vulkan applications.

# Build Sample & Test Apps
To build test and sample apps. You'll need to install Vulkan SDK and VMA memory manager.

On Linux, just run [bootstrap.sh](dev/env/bootstrap.sh) to install all dependencies.

On Windows, install Vulkan SDK from LunarG website, as well as cmake, git, python 3.8+.

After than run [env.sh](env.sh)/[env.cmd](env.cmd) to launch the dev console. Then type:
    - `b d` to build debug variant;
    - `b p` to build profile variant; and
    - `b r` to build release variant.

Type `b --help` for detail help.

After everything is built. You can run cit.sh/cit.cmd to run check-in-test suite.

# License
The library is released under MIT license. See [LICENSE](LICENSE) file for details.
