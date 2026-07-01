cmake_minimum_required(VERSION 3.20)

project(librfb_build)

add_subdirectory(hip_dummy)
add_subdirectory(cpp_helper)
add_subdirectory(win32_helper)
add_subdirectory(vulkan_helper)
add_subdirectory(vulkan_start)
add_subdirectory(posix_cpp)
add_subdirectory(xkb_helper)
add_subdirectory(librfb)

