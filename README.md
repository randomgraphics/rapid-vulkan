# rapid-vulkan
rapid-vulkan is a lightweight wrappers and utilities for rapid creation of Vulkan app. It is designed in a header-only manner that can be easiy integrated into any Vulkan based applications.

The library depends on the C++ version of the Vulkan header (vulkan.hpp) coming with Vulkan SDK, and one additional 3rd-party library: **spirv-reflect**, which is already included in the repository.

It is currently developed and tested on both Windows 11 and Ubuntu 22.04.

# Usage
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

# License
The library is released under MIT license. See [LICENSE](LICENSE) file for details.
