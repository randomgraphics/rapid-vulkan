set(CMAKE_CXX_STANDARD 17)

Include(CheckIncludeFileCXX)

function(add_sample name)
    add_executable(${name} ${name}.cpp)
    if (NOT (${name} STREQUAL "hello-world"))
        # Build other sample apps only after hello-world pass build. This is to avoid
        # spamming the console with repeated build errors when rapid-vulkan libray is failed to build.
        add_dependencies(${name} hello-world)
    endif()
endfunction()

# Check volk.h on Linux
if (UNIX)
    check_include_file_cxx(volk.h HAS_VOLK_H)
else()
    set(HAS_VOLK_H 1) # Assume volk.h exists on Windows
endif()

add_sample(hello-world)
add_sample(alternative-namespace)
if (HAS_VOLK_H)
    add_sample(use-with-volk-loader)
endif()
add_sample(noop-compute)
