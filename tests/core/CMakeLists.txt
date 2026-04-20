cmake_minimum_required(VERSION 3.14)

# Demuxer test
add_executable(demuxer_test demuxer_test.cpp)
target_link_libraries(demuxer_test PRIVATE avsdk_core GTest::gtest_main)
add_test(NAME demuxer_test COMMAND demuxer_test)

# Decoder test
add_executable(decoder_test decoder_test.cpp)
target_link_libraries(decoder_test PRIVATE avsdk_core GTest::gtest_main)
add_test(NAME decoder_test COMMAND decoder_test)
