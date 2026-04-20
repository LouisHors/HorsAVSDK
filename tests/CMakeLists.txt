cmake_minimum_required(VERSION 3.14)

enable_testing()

# Find GoogleTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Infrastructure tests
add_subdirectory(infrastructure)

# Core tests
add_subdirectory(core)
