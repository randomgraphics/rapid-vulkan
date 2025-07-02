# Setup Vulkan SDK
find_package(Vulkan REQUIRED)
message("
Vulkan_INCLUDE_DIRS = ${Vulkan_INCLUDE_DIRS}
Vulkan_LIBRARIES    = ${Vulkan_LIBRARIES}
")
include_directories(${Vulkan_INCLUDE_DIRS})
if (APPLE)
# Setup LC_RPATH on macos to point to where libvulkan.dylib is. So the executable can load it at runtime.
# FIXME: this ties the executable to the Vulkan SDK install location on the current build machine. It means
# the executable might not run on other machines.
get_filename_component(Vulkan_LIB_DIR ${Vulkan_LIBRARY} DIRECTORY)
set(CMAKE_BUILD_RPATH ${Vulkan_LIB_DIR})
endif()

# setup list of system specific dependencies
if (ANDROID)
    link_libraries(log)
else()
    # build glfw
    function(build_glfw)
        # Enclose build process inside a function. So we can freely change build
        # options w/o affecting the rest of the project
        remove_definitions()
        option(GLFW_BUILD_EXAMPLES "" OFF)
        option(GLFW_BUILD_TESTS "" OFF)
        option(GLFW_BUILD_DOCS "" OFF)
        option(GLFW_INSTALL "" OFF)
        add_subdirectory(3rd-party/glfw)
    endfunction()
    build_glfw()
    link_libraries(glfw)
endif()

# Setup common build flags. Do this after glfw build
if (MSVC)
    add_compile_options(/MP /W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wconversion -Wshadow -Werror -Wfatal-errors)
endif()
include_directories(../inc 3rd-party)
add_compile_definitions($<$<CONFIG:Debug>:RAPID_VULKAN_ENABLE_DEBUG_BUILD=1>)
set(CMAKE_CXX_STANDARD 17)

# Build rapid-vulkan into a static library
add_library(rapid-vulkan-static STATIC rv.cpp)

# Build subfolders
add_subdirectory(sample)
add_subdirectory(test)
add_subdirectory(experimental)
