set(CMAKE_CXX_STANDARD 17)

Include(CheckIncludeFileCXX)

function(add_sample name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} rapid-vulkan-static ${RAPID_VULKAN_SYSTEM_LIBRARIES})
endfunction()

function(add_sample_no_link_to_rv name)
    add_executable(${name} ${name}.cpp)
    add_dependencies(${name} rapid-vulkan-static) # Don't link to it. But still wait for it to be built.
endfunction()

# Check volk.h on Linux
if (UNIX)
    check_include_file_cxx(volk.h HAS_VOLK_H)
else()
    check_include_file_cxx(Volk/volk.h HAS_VOLK_H)
endif()

add_sample(hello-world)
add_sample_no_link_to_rv(alternative-namespace)
if (HAS_VOLK_H)
    add_sample_no_link_to_rv(use-with-volk-loader)
endif()
add_sample(noop-compute)
add_sample(pipeline-args)

if (NOT ANDROID)
    add_sample(simple-triangle)
endif()
