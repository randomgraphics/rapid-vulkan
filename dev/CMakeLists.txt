find_package(Vulkan REQUIRED)

# setup common build flags
include_directories(../inc ${Vulkan_INCLUDE_DIRS})
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()
if (UNIX)
    link_libraries(dl)
endif()

add_subdirectory(sample)
add_subdirectory(test)
