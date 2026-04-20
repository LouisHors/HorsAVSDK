cmake_minimum_required(VERSION 3.14)

# Core library
add_library(avsdk_core STATIC
    demuxer/ffmpeg_demuxer.cpp
    demuxer/network_demuxer.cpp
    decoder/ffmpeg_decoder.cpp
    player/player_impl.cpp
    buffer/ring_buffer.cpp
    hls/hls_parser.cpp
)

target_include_directories(avsdk_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(avsdk_core PUBLIC
    avsdk_infrastructure
    avsdk_network
    ${FFMPEG_LIBRARIES}
)
