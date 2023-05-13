# rapid-vulkan
rapid-vulkan is a lightweight wrappers and utilities for rapid creation of Vulkan app.

The library depends on the C++ version of the Vulkan header (vulkan.hpp) coming with Vulkan SDK, and one additional 3rd-party library: **spirv-reflect**, which is already included in the repository.

It is currently developed and tested on Windows platform. Linux support is on its way.

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

# Usage & Samples
Please checkout apps in [sample](dev/sample) folder for how to use rapid-vulkan to quickly create Vulkan applications.