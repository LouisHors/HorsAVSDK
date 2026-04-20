cmake_minimum_required(VERSION 3.14)

add_subdirectory(simple_player)

if(APPLE AND PLATFORM STREQUAL "macOS")
    add_subdirectory(macos_player)
endif()
