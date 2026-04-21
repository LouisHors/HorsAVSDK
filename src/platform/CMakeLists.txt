cmake_minimum_required(VERSION 3.14)

if(APPLE)
    # macOS platform library with Metal and VideoToolbox support
    set(PLATFORM_SOURCES
        macos/metal_renderer.mm
        macos/videotoolbox_decoder.mm
        macos/audio_unit_renderer.mm
    )

    add_library(avsdk_platform STATIC ${PLATFORM_SOURCES})

    target_include_directories(avsdk_platform PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${FFMPEG_INCLUDE_DIRS}
    )

    target_link_libraries(avsdk_platform PUBLIC
        ${FFMPEG_LIBRARIES}
        avsdk_infrastructure
        avsdk_core
        "-framework Metal"
        "-framework Cocoa"
        "-framework VideoToolbox"
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework AudioUnit"
    )

    target_compile_options(avsdk_platform PRIVATE
        -fobjc-arc
    )
endif()
