find_package(VULKAN REQUIRED)

include_directories(../../inc ${Vulkan_INCLUDE_DIRS})
set(CMAKE_CXX_STANDARD 17)

function(add_sample name)
    add_executable(${name} ${name}.cpp)
endfunction()

add_sample(built-in-loader)
add_sample(hello-world)
add_sample(noop-compute)
add_sample(use-with-volk)
