cmake_minimum_required(VERSION 3.14)

# Core library
add_library(avsdk_core STATIC
    demuxer/ffmpeg_demuxer.cpp
    demuxer/network_demuxer.cpp
    decoder/ffmpeg_decoder.cpp
    encoder/ffmpeg_encoder.cpp
    player/player_impl.cpp
    player/audio_clock.cpp
    buffer/ring_buffer.cpp
    hls/hls_parser.cpp
    network/retry_policy.cpp
    bypass/data_bypass_manager.cpp
    bypass/screenshot_handler.cpp
    bypass/video_recorder.cpp
    bypass/filter_processor.cpp
)

target_include_directories(avsdk_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(avsdk_core PUBLIC
    avsdk_infrastructure
    avsdk_network
    avsdk_platform
    ${FFMPEG_LIBRARIES}
)
