cmake_minimum_required(VERSION 3.14)

enable_testing()

# Find GoogleTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Infrastructure tests
add_subdirectory(infrastructure)

# Core tests
add_subdirectory(core)

# Platform tests
add_subdirectory(platform)

# Network tests
add_subdirectory(network)

# Integration tests
add_executable(player_integration_test integration/player_integration_test.cpp)
target_link_libraries(player_integration_test avsdk_core avsdk_platform avsdk_infrastructure ${FFMPEG_LIBRARIES} GTest::gtest GTest::gtest_main pthread)
add_test(NAME PlayerIntegrationTest COMMAND player_integration_test)
