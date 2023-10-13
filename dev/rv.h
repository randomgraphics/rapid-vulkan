#pragma once
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#define RAPID_VULKAN_LOG_ERROR(message)                                  \
    do {                                                                 \
        auto message___ = rapid_vulkan::format("[ERROR] %s\n", message); \
        fprintf(stderr, message___.c_str());                             \
        ::OutputDebugStringA(message___.c_str());                        \
    } while (false)
#define RAPID_VULKAN_LOG_WARNING(message)                                \
    do {                                                                 \
        auto message___ = rapid_vulkan::format("[WARN_] %s\n", message); \
        fprintf(stderr, message___.c_str());                             \
        ::OutputDebugStringA(message___.c_str());                        \
    } while (false)
#endif

// Enable GLFW3 support if not on Andorid or iOS.
#ifndef __ANDROID__
#define RAPID_VULKAN_ENABLE_GLFW3 1
#define RAPID_VULKAN_GLFW3_HEADER <glfw/include/GLFW/glfw3.h>
#endif

#include <rapid-vulkan/rapid-vulkan.h>

std::string backtrace();