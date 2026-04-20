cmake_minimum_required(VERSION 3.14)

# Demuxer test
add_executable(demuxer_test demuxer_test.cpp)
target_link_libraries(demuxer_test PRIVATE avsdk_core GTest::gtest_main)
add_test(NAME demuxer_test COMMAND demuxer_test)

# Player test
add_executable(player_test player_test.cpp)
target_link_libraries(player_test PRIVATE avsdk_core GTest::gtest_main)
add_test(NAME player_test COMMAND player_test)

# Decoder test
add_executable(decoder_test decoder_test.cpp)
target_link_libraries(decoder_test PRIVATE avsdk_core GTest::gtest_main)
add_test(NAME decoder_test COMMAND decoder_test)

# Ring buffer test
add_executable(ring_buffer_test ring_buffer_test.cpp)
target_link_libraries(ring_buffer_test PRIVATE avsdk_core GTest::gtest_main)
add_test(NAME ring_buffer_test COMMAND ring_buffer_test)

# HLS parser test
add_executable(hls_parser_test hls_parser_test.cpp)
target_link_libraries(hls_parser_test PRIVATE avsdk_core GTest::gtest_main)
add_test(NAME hls_parser_test COMMAND hls_parser_test)

# Network demuxer test
add_executable(network_demuxer_test network_demuxer_test.cpp)
target_link_libraries(network_demuxer_test PRIVATE avsdk_core GTest::gtest_main)
add_test(NAME network_demuxer_test COMMAND network_demuxer_test)

# Retry policy test
add_executable(retry_policy_test retry_policy_test.cpp)
target_link_libraries(retry_policy_test PRIVATE avsdk_core GTest::gtest_main)
add_test(NAME retry_policy_test COMMAND retry_policy_test)
