#pragma once

// HorsAVDataBypass.h
// Data bypass for HorsAVSDK

#import <Foundation/Foundation.h>
#import "HorsAVTypes.h"

@class HorsAVPlayer;

NS_ASSUME_NONNULL_BEGIN

// MARK: - Data Bypass Delegate

/**
 * Protocol for receiving data bypass callbacks
 *
 * Implement this protocol to receive raw decoded frames or encoded data
 * for custom processing, analysis, or recording.
 */
NS_SWIFT_NAME(DataBypassDelegate)
@protocol HorsAVDataBypassDelegate <NSObject>

@optional

/**
 * Called when a video frame is received at the specified stage
 * @param player The player instance
 * @param frame The video frame data
 * @param stage The bypass stage
 */
- (void)player:(HorsAVPlayer *)player
    didReceiveVideoFrame:(HorsAVVideoFrame *)frame
                   atStage:(HorsAVDataBypassStage)stage
    NS_SWIFT_NAME(player(_:didReceiveVideoFrame:atStage:));

/**
 * Called when an audio frame is received at the specified stage
 * @param player The player instance
 * @param frame The audio frame data
 * @param stage The bypass stage
 */
- (void)player:(HorsAVPlayer *)player
    didReceiveAudioFrame:(HorsAVAudioFrame *)frame
                   atStage:(HorsAVDataBypassStage)stage
    NS_SWIFT_NAME(player(_:didReceiveAudioFrame:atStage:));

/**
 * Called when a raw encoded packet is received
 * @param player The player instance
 * @param data Raw encoded data
 * @param timestamp Timestamp in milliseconds
 * @param isKeyFrame Whether this is a key frame
 */
- (void)player:(HorsAVPlayer *)player
    didReceiveEncodedData:(NSData *)data
                timestamp:(int64_t)timestamp
               isKeyFrame:(BOOL)isKeyFrame
    NS_SWIFT_NAME(player(_:didReceiveEncodedData:timestamp:isKeyFrame:));

@end

// MARK: - Data Bypass Configuration

/**
 * Configuration for data bypass
 */
NS_SWIFT_NAME(DataBypassConfiguration)
@interface HorsAVDataBypassConfiguration : NSObject <NSCopying>

/**
 * Enable video frame callbacks. Default is NO.
 */
@property (nonatomic, readwrite) BOOL enableVideoFrames;

/**
 * Enable audio frame callbacks. Default is NO.
 */
@property (nonatomic, readwrite) BOOL enableAudioFrames;

/**
 * Enable encoded data callbacks. Default is NO.
 */
@property (nonatomic, readwrite) BOOL enableEncodedData;

/**
 * The bypass stage to receive callbacks at. Default is Decoded.
 */
@property (nonatomic, readwrite) HorsAVDataBypassStage bypassStage;

/**
 * Maximum callback frequency in Hz. 0 means no limit. Default is 30.
 * Useful for reducing CPU usage when only periodic snapshots are needed.
 */
@property (nonatomic, readwrite) NSInteger maxCallbackFrequency;

/**
 * Default configuration with all bypass disabled
 */
+ (instancetype)defaultConfiguration NS_SWIFT_NAME(default());

@end

NS_ASSUME_NONNULL_END
