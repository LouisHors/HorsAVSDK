#pragma once

// HorsAVPlayerInternal.h
// Internal implementation details for HorsAVPlayer

#import <Foundation/Foundation.h>
#import "HorsAVPlayer.h"
#import <memory>

// C++ forward declarations
namespace avsdk {
    class IPlayer;
    class DataBypassManager;
}

@class HorsAVCallbackBridge;

NS_ASSUME_NONNULL_BEGIN

/**
 * Internal interface for HorsAVPlayer
 * This category is used for implementation only
 */
@interface HorsAVPlayer (Internal)

/// C++ player instance
@property (nonatomic, readonly) std::shared_ptr<avsdk::IPlayer> cppPlayer;

/// Callback bridge
@property (nonatomic, strong) HorsAVCallbackBridge *callbackBridge;

/// Initialize with existing C++ player (for testing)
- (instancetype)initWithCPPPlayer:(std::shared_ptr<avsdk::IPlayer>)player;

// Internal write access to readonly properties
@property (nonatomic, readwrite) NSTimeInterval currentTime;
@property (nonatomic, readwrite, getter=isBuffering) BOOL buffering;
@property (nonatomic, readwrite) NSInteger bufferingPercent;

@end

NS_ASSUME_NONNULL_END
