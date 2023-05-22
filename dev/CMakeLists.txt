find_package(Vulkan REQUIRED)

# setup common build flags
include_directories(../inc ${Vulkan_INCLUDE_DIRS})
if (MSVC)
    add_compile_options(/MP /W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()
if (UNIX)
    link_libraries(dl)
endif()

# build glfw
option(GLFW_BUILD_EXAMPLES "" OFF)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
add_subdirectory(3rd-party/glfw)

# Build rapid-vulkan into a static library
add_library(rapid-vulkan-static STATIC rv.cpp)
target_compile_features(rapid-vulkan-static PUBLIC cxx_std_17)
target_compile_definitions(rapid-vulkan-static PUBLIC  $<$<CONFIG:Debug>:RAPID_VULKAN_ENABLE_DEBUG_BUILD=1>)

add_subdirectory(sample)
add_subdirectory(test)
