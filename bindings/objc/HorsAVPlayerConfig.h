#pragma once

// HorsAVPlayerConfig.h
// Configuration classes for HorsAVSDK

#import <Foundation/Foundation.h>
#import "HorsAVTypes.h"

NS_ASSUME_NONNULL_BEGIN

// MARK: - Player Configuration

/**
 * Configuration for HorsAVPlayer
 *
 * Use this class to configure player behavior before initialization.
 * Configuration is immutable once applied; create a new instance to change settings.
 */
NS_SWIFT_NAME(PlayerConfiguration)
@interface HorsAVPlayerConfiguration : NSObject <NSCopying, NSMutableCopying>

/**
 * Decoder mode. Default is HardwareFirst.
 */
@property (nonatomic, readwrite) HorsAVDecoderMode decoderMode;

/**
 * Enable hardware decoder. Default is YES.
 * If hardware decoder fails, will fallback to software.
 */
@property (nonatomic, readwrite) BOOL enableHardwareDecoder;

/**
 * Buffer time in seconds. Default is 1.0.
 * Larger values provide smoother playback but more delay.
 */
@property (nonatomic, readwrite) NSTimeInterval bufferTime;

/**
 * Log level. Default is Info.
 */
@property (nonatomic, readwrite, copy) NSString *logLevel;

/**
 * Enable automatic audio resampling. Default is YES.
 * Converts any input format to device-supported format.
 */
@property (nonatomic, readwrite) BOOL enableAudioResampling;

/**
 * Enable audio-video synchronization. Default is YES.
 */
@property (nonatomic, readwrite) BOOL enableAVSync;

/**
 * Maximum allowed AV sync drift in milliseconds. Default is 40.
 */
@property (nonatomic, readwrite) NSInteger maxSyncDriftMs;

/**
 * Default configuration with recommended settings
 */
+ (instancetype)defaultConfiguration NS_SWIFT_NAME(default());

/**
 * Configuration optimized for low latency
 */
+ (instancetype)lowLatencyConfiguration NS_SWIFT_NAME(lowLatency());

/**
 * Configuration optimized for smooth playback
 */
+ (instancetype)smoothPlaybackConfiguration NS_SWIFT_NAME(smoothPlayback());

@end

NS_ASSUME_NONNULL_END
