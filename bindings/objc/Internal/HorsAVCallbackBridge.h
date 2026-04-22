#pragma once

// HorsAVCallbackBridge.h
// Internal C++ to Objective-C callback bridge

#import <Foundation/Foundation.h>
#import <memory>

// Forward declarations for C++ types
namespace avsdk {
    class IPlayerCallback;
    class IDataBypass;
    struct MediaInfo;
    enum class PlayerState;
    enum class ErrorCode;
    using Timestamp = int64_t;
}

@class HorsAVPlayer;

NS_ASSUME_NONNULL_BEGIN

/**
 * Internal callback bridge from C++ to Objective-C
 * This class is used internally and not exposed in public headers
 */
@interface HorsAVCallbackBridge : NSObject

- (instancetype)initWithPlayer:(HorsAVPlayer *)player NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Get the C++ callback interface
- (std::shared_ptr<avsdk::IPlayerCallback>)playerCallback;

@end

NS_ASSUME_NONNULL_END
