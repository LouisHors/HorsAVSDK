cmake_minimum_required(VERSION 3.14)

# macOS Player Demo CMake Configuration

# Set Objective-C/C++ flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-arc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc -std=c++14")

# Source files (to be filled in later tasks)
set(MACOS_PLAYER_SOURCES
    # Will be populated in subsequent tasks - placeholder for now
    "${CMAKE_CURRENT_SOURCE_DIR}/placeholder.mm"
)

# Header files (to be filled in later tasks)
set(MACOS_PLAYER_HEADERS
    # Will be populated in subsequent tasks
)

# Create macOS app bundle
add_executable(macos_player MACOSX_BUNDLE
    ${MACOS_PLAYER_SOURCES}
    ${MACOS_PLAYER_HEADERS}
)

# Set Info.plist properties
set_target_properties(macos_player PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "HorsAVPlayer"
    MACOSX_BUNDLE_BUNDLE_IDENTIFIER "com.hors.avplayer"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.hors.avplayer"
)

# Include directories
target_include_directories(macos_player PRIVATE
    ${CMAKE_SOURCE_DIR}/include/avsdk
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link directories
link_directories(${CMAKE_BINARY_DIR}/src)

# Link libraries
target_link_libraries(macos_player PRIVATE
    avsdk_core
    avsdk_platform
)

# Link macOS frameworks
find_library(COCOA_LIBRARY Cocoa)
find_library(METAL_LIBRARY Metal)
find_library(METALKIT_LIBRARY MetalKit)
find_library(COREMEDIA_LIBRARY CoreMedia)
find_library(COREVIDEO_LIBRARY CoreVideo)
find_library(AVFOUNDATION_LIBRARY AVFoundation)

if(COCOA_LIBRARY)
    target_link_libraries(macos_player PRIVATE ${COCOA_LIBRARY})
endif()
if(METAL_LIBRARY)
    target_link_libraries(macos_player PRIVATE ${METAL_LIBRARY})
endif()
if(METALKIT_LIBRARY)
    target_link_libraries(macos_player PRIVATE ${METALKIT_LIBRARY})
endif()
if(COREMEDIA_LIBRARY)
    target_link_libraries(macos_player PRIVATE ${COREMEDIA_LIBRARY})
endif()
if(COREVIDEO_LIBRARY)
    target_link_libraries(macos_player PRIVATE ${COREVIDEO_LIBRARY})
endif()
if(AVFOUNDATION_LIBRARY)
    target_link_libraries(macos_player PRIVATE ${AVFOUNDATION_LIBRARY})
endif()

# Set output name
set_target_properties(macos_player PROPERTIES
    OUTPUT_NAME "HorsAVPlayer"
)
