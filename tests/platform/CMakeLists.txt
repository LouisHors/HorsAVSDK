cmake_minimum_required(VERSION 3.14)

# Platform-specific tests
if(APPLE)
    # Metal renderer test for macOS
    add_executable(metal_renderer_test metal_renderer_test.mm)
    target_link_libraries(metal_renderer_test PRIVATE
        avsdk_platform
        GTest::gtest
        GTest::gtest_main
        "-framework Metal"
        "-framework Cocoa"
    )
    target_include_directories(metal_renderer_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${FFMPEG_INCLUDE_DIRS}
    )
    add_test(NAME MetalRendererTest COMMAND metal_renderer_test)

    # VideoToolbox decoder test for macOS
    add_executable(videotoolbox_decoder_test videotoolbox_decoder_test.mm)
    target_link_libraries(videotoolbox_decoder_test PRIVATE
        avsdk_platform
        avsdk_infrastructure
        GTest::gtest
        GTest::gtest_main
        "-framework VideoToolbox"
        "-framework CoreVideo"
        "-framework CoreMedia"
    )
    target_include_directories(videotoolbox_decoder_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${FFMPEG_INCLUDE_DIRS}
    )
    add_test(NAME VideoToolboxDecoderTest COMMAND videotoolbox_decoder_test)
endif()
