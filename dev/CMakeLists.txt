find_package(Vulkan REQUIRED)
include_directories(../inc ${Vulkan_INCLUDE_DIRS})
add_subdirectory(sample)
add_subdirectory(test)
