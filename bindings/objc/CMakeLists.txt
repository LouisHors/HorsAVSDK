# CMakeLists.txt for Objective-C bindings
# This builds the HorsAVSDK Objective-C framework for macOS/iOS

cmake_minimum_required(VERSION 3.14)

# Only build for Apple platforms
if(NOT APPLE)
    message(STATUS "Objective-C bindings are only available on Apple platforms")
    return()
endif()

# Find frameworks
find_library(FOUNDATION_LIBRARY Foundation)
find_library(METALKIT_LIBRARY MetalKit)
find_library(METAL_LIBRARY Metal)
find_library(COREGRAPHICS_LIBRARY CoreGraphics)

# Collect Objective-C binding source files
set(HORSAV_OBJC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVSDK.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVPlayer.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVPlayerConfig.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVTypes.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVError.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVDataBypass.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVRenderer.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVLogger.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/Internal/HorsAVCallbackBridge.mm
)

# Collect headers
set(HORSAV_OBJC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVSDK.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVPlayer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVPlayerConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVError.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVDataBypass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVLogger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/Internal/HorsAVPlayerInternal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/Internal/HorsAVCallbackBridge.h
)

# Create Objective-C library
add_library(avsdk_objc STATIC ${HORSAV_OBJC_SOURCES} ${HORSAV_OBJC_HEADERS})

target_include_directories(avsdk_objc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/Internal
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(avsdk_objc
    avsdk_core
    avsdk_platform
    ${FOUNDATION_LIBRARY}
    ${METALKIT_LIBRARY}
    ${METAL_LIBRARY}
    ${COREGRAPHICS_LIBRARY}
)

# Set Objective-C++ compilation flags
set_target_properties(avsdk_objc PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    COMPILE_FLAGS "-fobjc-arc -std=c++14"
)

# Add framework for macOS demo
if(PLATFORM STREQUAL "macOS")
    add_library(HorsAVSDK SHARED ${HORSAV_OBJC_SOURCES} ${HORSAV_OBJC_HEADERS})

    set_target_properties(HorsAVSDK PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION A
        MACOSX_FRAMEWORK_IDENTIFIER com.horsav.sdk
        MACOSX_FRAMEWORK_BUNDLE_VERSION "1.0.0"
        MACOSX_FRAMEWORK_SHORT_VERSION_STRING "1.0.0"
        PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVSDK.h;${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVPlayer.h;${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVPlayerConfig.h;${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVTypes.h;${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVError.h;${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVDataBypass.h;${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVRenderer.h;${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc/HorsAVLogger.h"
        COMPILE_FLAGS "-fobjc-arc -std=c++14"
    )

    target_include_directories(HorsAVSDK PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/bindings/objc
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(HorsAVSDK
        avsdk_core
        avsdk_platform
        ${FOUNDATION_LIBRARY}
        ${METALKIT_LIBRARY}
        ${METAL_LIBRARY}
        ${COREGRAPHICS_LIBRARY}
    )
endif()
