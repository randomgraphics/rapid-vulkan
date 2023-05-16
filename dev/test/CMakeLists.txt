set(CMAKE_CXX_STANDARD 20)
add_executable(rapid-vulkan-test
    buffer-test.cpp
    image-test.cpp
    test-main.cpp
)
target_compile_definitions(rapid-vulkan-test PUBLIC  $<$<CONFIG:Debug>:RAPID_VULKAN_ENABLE_DEBUG_BUILD=1>)
# Build test app only after hello-world is properly built to avoid spamming the console with redundant error messages.
add_dependencies(rapid-vulkan-test hello-world)
