#pragma once

// HorsAVPlayer.h
// Main player class for HorsAVSDK

#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>
#import "HorsAVTypes.h"

@class HorsAVPlayerConfiguration;
@class HorsAVDataBypassConfiguration;
@protocol HorsAVPlayerDelegate;
@protocol HorsAVDataBypassDelegate;

NS_ASSUME_NONNULL_BEGIN

// MARK: - HorsAVPlayer

/**
 * Main player class for HorsAVSDK
 *
 * This class provides a high-level Objective-C interface to the HorsAVSDK
 * media playback functionality. It supports local files and network streams,
 * hardware decoding, and data bypass callbacks.
 *
 * Thread safety: Most methods are not thread-safe and should be called from
 * the main thread only. Delegate callbacks are dispatched to the main queue.
 */
NS_SWIFT_NAME(Player)
@interface HorsAVPlayer : NSObject

#pragma mark - Initialization

/**
 * Initialize with default configuration
 * @return Player instance, or nil if initialization failed
 */
- (nullable instancetype)init NS_SWIFT_NAME(init());

/**
 * Initialize with custom configuration
 * @param configuration The player configuration
 * @param error Error output if initialization fails
 * @return Player instance, or nil if initialization failed
 */
- (nullable instancetype)initWithConfiguration:(nullable HorsAVPlayerConfiguration *)configuration
                                         error:(NSError **)error
    NS_SWIFT_NAME(init(configuration:)) NS_DESIGNATED_INITIALIZER;

+ (instancetype)new NS_UNAVAILABLE;

#pragma mark - Properties

/**
 * Player delegate for receiving state change and progress callbacks
 */
@property (nonatomic, weak, nullable) id<HorsAVPlayerDelegate> delegate;

/**
 * Current playback state
 */
@property (nonatomic, readonly) HorsAVPlayerState state;

/**
 * Current playback position in seconds
 */
@property (nonatomic, readonly) NSTimeInterval currentTime;

/**
 * Media total duration in seconds. 0 if unknown or no media opened.
 */
@property (nonatomic, readonly) NSTimeInterval duration;

/**
 * Video size in pixels. CGSizeZero if no video or unknown.
 */
@property (nonatomic, readonly) CGSize videoSize;

/**
 * Whether the player is currently buffering
 */
@property (nonatomic, readonly, getter=isBuffering) BOOL buffering;

/**
 * Buffering percentage (0-100). Only valid when isBuffering is YES.
 */
@property (nonatomic, readonly) NSInteger bufferingPercent;

/**
 * Whether media is currently loaded and ready to play
 */
@property (nonatomic, readonly) BOOL isReady;

#pragma mark - Playback Control

/**
 * Open a media file or stream
 * @param url Local file URL or remote stream URL
 * @param error Error output if open fails
 * @return YES if successful, NO otherwise
 */
- (BOOL)openURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(open(_:));

/**
 * Start or resume playback
 */
- (void)play NS_SWIFT_NAME(play());

/**
 * Pause playback
 */
- (void)pause NS_SWIFT_NAME(pause());

/**
 * Stop playback and reset to stopped state
 */
- (void)stop NS_SWIFT_NAME(stop());

/**
 * Seek to a specific time
 * @param time Target time in seconds
 * @param completionHandler Called when seek completes or fails
 */
- (void)seekToTime:(NSTimeInterval)time
 completionHandler:(nullable void (^)(BOOL success, NSTimeInterval actualTime, NSError *_Nullable error))completionHandler
    NS_SWIFT_NAME(seek(to:completionHandler:));

/**
 * Close the current media and release resources
 */
- (void)close NS_SWIFT_NAME(close());

#pragma mark - Rendering

/**
 * Set the view for video rendering
 * @param view NSView to render video to. SDK will create and manage MTKView internally.
 */
- (void)setRenderView:(nullable NSView *)view NS_SWIFT_NAME(setRenderView(_:));

/**
 * Set the render mode
 * @param mode The render mode
 */
- (void)setRenderMode:(HorsAVRenderMode)mode NS_SWIFT_NAME(setRenderMode(_:));

#pragma mark - Audio Tracks

/**
 * Available audio tracks for the currently opened media.
 * Returns nil if no media is loaded or no audio is present.
 */
@property (nonatomic, readonly, nullable, copy) NSArray<HorsAVAudioTrackInfo *> *audioTracks;

/**
 * Index of the currently selected audio track
 */
@property (nonatomic, readonly) NSInteger selectedAudioTrack;

/**
 * Select an audio track by index
 * @param trackIndex The track index from audioTracks array
 * @param error Error output if selection fails
 * @return YES if successful, NO otherwise
 */
- (BOOL)selectAudioTrack:(NSInteger)trackIndex error:(NSError **)error
    NS_SWIFT_NAME(selectAudioTrack(_:));

/**
 * When YES, all audio tracks are mixed and played simultaneously.
 * When NO (default), only the selected track is played.
 */
@property (nonatomic, readwrite, getter=isMixAllAudioTracks) BOOL mixAllAudioTracks;

#pragma mark - Data Bypass

/**
 * Configure data bypass
 * @param configuration Data bypass configuration
 */
- (void)setDataBypassConfiguration:(nullable HorsAVDataBypassConfiguration *)configuration
    NS_SWIFT_NAME(setDataBypassConfiguration(_:));

/**
 * Data bypass delegate for receiving frame callbacks
 */
@property (nonatomic, weak, nullable) id<HorsAVDataBypassDelegate> dataBypassDelegate;

@end

// MARK: - Player Delegate

/**
 * Delegate protocol for HorsAVPlayer
 *
 * Implement this protocol to receive player state changes, progress updates,
 * and error notifications. All methods are optional and called on main queue.
 */
NS_SWIFT_NAME(PlayerDelegate)
@protocol HorsAVPlayerDelegate <NSObject>

@optional

/**
 * Called when media is prepared and ready to play
 * @param player The player instance
 * @param info Media information
 */
- (void)player:(HorsAVPlayer *)player
    didPrepareMedia:(HorsAVMediaInfo *)info
    NS_SWIFT_NAME(player(_:didPrepareMedia:));

/**
 * Called when player state changes
 * @param player The player instance
 * @param oldState Previous state
 * @param newState Current state
 */
- (void)player:(HorsAVPlayer *)player
 didChangeState:(HorsAVPlayerState)oldState
        toState:(HorsAVPlayerState)newState
    NS_SWIFT_NAME(player(_:didChangeState:toState:));

/**
 * Called when an error occurs
 * @param player The player instance
 * @param error Error information
 */
- (void)player:(HorsAVPlayer *)player
 didEncounterError:(NSError *)error
    NS_SWIFT_NAME(player(_:didEncounterError:));

/**
 * Called when playback completes (reaches end of stream)
 * @param player The player instance
 */
- (void)playerDidCompletePlayback:(HorsAVPlayer *)player
    NS_SWIFT_NAME(playerDidCompletePlayback(_:));

/**
 * Called periodically with playback progress
 * @param player The player instance
 * @param currentTime Current playback position
 * @param duration Total media duration
 */
- (void)player:(HorsAVPlayer *)player
 didUpdateProgress:(NSTimeInterval)currentTime
          duration:(NSTimeInterval)duration
    NS_SWIFT_NAME(player(_:didUpdateProgress:duration:));

/**
 * Called when buffering state changes
 * @param player The player instance
 * @param isBuffering Whether buffering is active
 * @param percent Buffering percentage (0-100)
 */
- (void)player:(HorsAVPlayer *)player
didChangeBuffering:(BOOL)isBuffering
           percent:(NSInteger)percent
    NS_SWIFT_NAME(player(_:didChangeBuffering:percent:));

/**
 * Called when seek operation completes
 * @param player The player instance
 * @param position Actual position after seek
 */
- (void)player:(HorsAVPlayer *)player
didCompleteSeekTo:(NSTimeInterval)position
    NS_SWIFT_NAME(player(_:didCompleteSeekTo:));

@end

NS_ASSUME_NONNULL_END
