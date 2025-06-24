#pragma once
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#define RAPID_VULKAN_LOG(severity, prefix, message)                                                            \
    do {                                                                                                       \
        auto message___ = rapid_vulkan::format("%s%s\n", prefix, message);                                     \
        fprintf((severity) < RAPID_VULKAN_NAMESPACE::LogSeverity::INFO ? stderr : stdout, message___.c_str()); \
        ::OutputDebugStringA(message___.c_str());                                                              \
    } while (false)
#endif

// Enable GLFW3 support if not on Andorid or iOS.
#ifndef __ANDROID__
#define RAPID_VULKAN_ENABLE_GLFW3 1
#define RAPID_VULKAN_GLFW3_HEADER <glfw/include/GLFW/glfw3.h>
#endif

#include <rapid-vulkan/rapid-vulkan.h>

std::string backtrace();