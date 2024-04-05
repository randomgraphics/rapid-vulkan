Include(CheckIncludeFileCXX)

function(add_sample name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} rapid-vulkan-static)
endfunction()

function(add_sample_no_link_to_rv name)
    add_executable(${name} ${name}.cpp)
    add_dependencies(${name} rapid-vulkan-static) # Don't link to it. But still wait for it to be built.
endfunction()

function(check_win_volk)
    # set this variable inside a function. So it doesn't leak out to affect the rest of the build.
    message(STATUS "Checking for volk.h. Vulkan_INCLUDE_DIRS = ${Vulkan_INCLUDE_DIRS}")
    set (CMAKE_REQUIRED_INCLUDES ${Vulkan_INCLUDE_DIRS})
    check_include_file_cxx(Volk/volk.h HAS_VOLK_H)
endfunction()

# Check volk.h on Linux
if (UNIX)
    check_include_file_cxx(volk.h HAS_VOLK_H)
else()
    check_win_volk()
endif()

add_sample(hello-world)
add_sample_no_link_to_rv(alternative-namespace)
add_sample(noop-compute)

if (NOT ANDROID)
    add_sample(drawable)
    add_sample(triangle)
endif()
