find_package(Vulkan REQUIRED)

# setup common build flags
include_directories(../inc ${Vulkan_INCLUDE_DIRS})

# setup list of system specific dependencies
if (ANDROID)
    set(RAPID_VULKAN_SYSTEM_LIBRARIES log)
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
    set(RAPID_VULKAN_SYSTEM_LIBRARIES glfw)
endif()

# Setup common build flags. Do this after glfw build
if (MSVC)
    add_compile_options(/MP /W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -Wconversion -Wshadow)
endif()
if (UNIX)
    link_libraries(dl)
endif()


# Build rapid-vulkan into a static library
add_library(rapid-vulkan-static STATIC rv.cpp)
target_compile_features(rapid-vulkan-static PUBLIC cxx_std_17)
target_include_directories(rapid-vulkan-static PUBLIC 3rd-party)
target_compile_definitions(rapid-vulkan-static PUBLIC $<$<CONFIG:Debug>:RAPID_VULKAN_ENABLE_DEBUG_BUILD=1>)

add_subdirectory(sample)
add_subdirectory(test)
