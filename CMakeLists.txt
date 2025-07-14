cmake_minimum_required(VERSION 3.13)
project(zero_copy_camera)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)

include_directories(
    ${LIBCAMERA_INCLUDE_DIRS}
    include
)

link_directories(${LIBCAMERA_LIBRARY_DIRS})

add_executable(camera_cli
    src/ZeroCopyCamera.cpp
)

target_link_libraries(camera_cli
    ${LIBCAMERA_LIBRARIES}
)

# Optional logging time util (header-only)
# Logger and ScopedTimer can be added in include/
