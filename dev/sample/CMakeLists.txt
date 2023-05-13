set(CMAKE_CXX_STANDARD 17)

function(add_sample name)
    add_executable(${name} ${name}.cpp)
    if (NOT (${name} STREQUAL "hello-world"))
        # Build other sample apps only after hello-world pass build. This is to avoid
        # spamming the console with repeated build errors when rapid-vulkan libray is failed to build.
        add_dependencies(${name} hello-world)
    endif()
endfunction()

add_sample(hello-world)
add_sample(external-loader)
add_sample(noop-compute)

