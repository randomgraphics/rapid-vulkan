cmake_minimum_required(VERSION 3.16)

project(rapid-vulkan)

option(RAPID_VULKAN_USE_VOLK "Use Volk/volk.h coming with Vulkan SDK 1.2.211+" ON)

find_package(Vulkan REQUIRED)

message(STATUS "Vulkan version: ${Vulkan_VERSION}")

if (${RAPID_VULKAN_USE_VOLK} AND (${Vulkan_VERSION} VERSION_LESS "1.2.211"))
    message(FATAL_ERROR "[ERROR] Current Vulkan SDK version (${Vulkan_VERSION}) is too old. Please update to 1.2.211 or newer for volk.h")
endif()

add_library(${PROJECT_NAME} src/rapid-vulkan.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC inc)
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_compile_definitions(${PROJECT_NAME} PUBLIC RAPID_VULKAN_USE_VOLK=$<${RAPID_VULKAN_USE_VOLK}?1:0>)
