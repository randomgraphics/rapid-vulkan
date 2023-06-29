set(CMAKE_CXX_STANDARD 20)
add_executable(rapid-vulkan-test
    buffer-test.cpp
    compute-test.cpp
    drawable-test.cpp
    graphics-test.cpp
    image-test.cpp
    perf-test.cpp
    queue-test.cpp
    sample-test.cpp
    smoke-test.cpp
    test-main.cpp
)
target_link_libraries(rapid-vulkan-test PUBLIC rapid-vulkan-static ${RAPID_VULKAN_SYSTEM_LIBRARIES})
