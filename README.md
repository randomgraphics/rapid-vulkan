# rapid-vulkan
rapid-vulkan is a lightweight wrappers and utilities for rapid creation of Vulkan app.

The library only depends on Vulkan SDK and one 3rd-party library: **spirv-reflect**, which is already included in the [this](src/3rd-party/spriv-reflect/) folder.

It is currently developed and tested on Windows platform. Linux support is on its way.

# Integration
The library is aiming for easy integration with any Vulkan project. Everything you need is included in [rapid-vulkan.h](src/rapid-vulkan.h).

Here are the step by step instructions to integrate it with your project:

1. Copy everything in [src](src) folder into your project's source folder.
2. Include [rapid-vulkan.h](src/rapid-vulkan.h) anywhere you need rapid-vulkan functionality.
3. In **one and only one** of your source files, include [rapid-vulkan.h](src/rapid-vulkan.h) with RAPID_VULKAN_IMPLEMENTATION macro defined in front of it. Then you are good to go:

```c
RAPID_VULKAN_IMPLEMENTATION
#include "rapid-vulkan.h"
```
