cmake_minimum_required(VERSION 3.14)

# Demuxer test
add_executable(demuxer_test demuxer_test.cpp)
target_link_libraries(demuxer_test PRIVATE avsdk_core GTest::gtest_main)
add_test(NAME demuxer_test COMMAND demuxer_test)
