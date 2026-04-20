cmake_minimum_required(VERSION 3.14)

# HTTP client test
add_executable(http_client_test http_client_test.cpp)
target_link_libraries(http_client_test PRIVATE avsdk_network GTest::gtest_main)
add_test(NAME http_client_test COMMAND http_client_test)
