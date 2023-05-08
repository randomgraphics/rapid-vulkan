# rapid-vulkan
rapid-vulkan is a lightweight wrappers and utilities for rapid creation of Vulkan app.

The library only depends on Vulkan SDK and one 3rd-party library: **spirv-reflect**, which is already included in the [this](src/3rd-party/spriv-reflect/) folder.

It is currently developed and tested on Windows platform. Linux support is on its way.

# Integration
The library is aiming for easy integration with any Vulkan project. Simply copy everything in [src](src) folder to your project. Then you are pretty much done.

You can add [rapid-vulkan.cpp](src/rapid-vulkan.cpp) into your build system, or directly include it in one of your source files:

```c
#include <rapid-vulkan.cpp>.
```

Then you can use all rapid-vulkan functionalies via [rapid-vulkan.h](src/rapid-vulkan.h).

Alternatively, you can also include [rapid-vulkan.h](src/rapid-vulkan.h) with **RAPID_VULKAN_IMPLEMENTATION** macro defined before the header file:

```c
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan.h>
```
