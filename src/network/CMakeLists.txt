cmake_minimum_required(VERSION 3.14)

# Find libcurl
find_package(CURL REQUIRED)

# HTTP client library
add_library(avsdk_network http_client.cpp)
target_include_directories(avsdk_network PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)
target_link_libraries(avsdk_network PUBLIC
    avsdk_infrastructure
    ${CURL_LIBRARIES}
)
