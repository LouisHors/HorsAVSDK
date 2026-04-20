cmake_minimum_required(VERSION 3.14)

# Infrastructure library
add_library(avsdk_infrastructure STATIC
    logger.cpp
    memory_pool.cpp
)

target_include_directories(avsdk_infrastructure PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
