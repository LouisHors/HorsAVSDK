cmake_minimum_required(VERSION 3.14)
project(HorsAVSDK VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale libswresample)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
)

# Library directories
link_directories(${FFMPEG_LIBRARY_DIRS})

# Core library
add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/infrastructure)

# Tests
enable_testing()
add_subdirectory(tests)

# Examples
add_subdirectory(examples)
