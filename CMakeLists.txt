cmake_minimum_required(VERSION 3.20)

project(librfb_build)

include(CTest)

add_subdirectory(cpp_helper)
add_subdirectory(socket_helper)
set(_librfb_build_testing "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
add_subdirectory(remote-media)
set(BUILD_TESTING "${_librfb_build_testing}")
unset(_librfb_build_testing)

# The window demo's GPU path is built on Wayland, DRM/amdgpu and HIP external
# memory, none of which exist on Windows. Everything below is Linux only; the
# protocol library and the headless tools build on both platforms.
if(UNIX)
    add_subdirectory(hip_helper)
    add_subdirectory(wayland_import)
    add_subdirectory(wayland_helper)
    add_subdirectory(drm_xxxx)
    add_subdirectory(posix_cpp)
    add_subdirectory(xkb_helper)
endif()

add_subdirectory(librfb)
