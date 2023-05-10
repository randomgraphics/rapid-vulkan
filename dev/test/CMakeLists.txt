find_package(VULKAN REQUIRED)

include_directories(../../inc ${Vulkan_INCLUDE_DIRS})
set(CMAKE_CXX_STANDARD 17)

function(add_test_app name)
    add_executable(${name} ${name}.cpp)
endfunction()

add_test_app(built-in-loader)
add_test_app(with-volk)
