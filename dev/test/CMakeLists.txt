set(CMAKE_CXX_STANDARD 20)
add_executable(rapid-vulkan-test
    buffer-test.cpp
    test-main.cpp
)
# Build test app only after hello-world is properly built to avoid spamming the console with redundant error messages.
add_dependencies(rapid-vulkan-test hello-world)
