#pragma once

// HorsAVTypes.h
// Core type definitions for HorsAVSDK

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

NS_ASSUME_NONNULL_BEGIN

// MARK: - Player State

/**
 * Player state enumeration
 */
NS_SWIFT_NAME(PlayerState)
typedef NS_ENUM(NSInteger, HorsAVPlayerState) {
    /**
     * Player is idle, not initialized
     */
    HorsAVPlayerStateIdle     = 0,
    /**
     * Player is stopped, ready to open media
     */
    HorsAVPlayerStateStopped  = 1,
    /**
     * Player is currently playing
     */
    HorsAVPlayerStatePlaying  = 2,
    /**
     * Player is paused
     */
    HorsAVPlayerStatePaused   = 3,
    /**
     * Player encountered an error
     */
    HorsAVPlayerStateError    = 4
} NS_SWIFT_NAME(PlayerState);

// MARK: - Decoder Mode

/**
 * Decoder mode for video decoding
 */
NS_SWIFT_NAME(DecoderMode)
typedef NS_ENUM(NSInteger, HorsAVDecoderMode) {
    /**
     * Automatically choose best decoder
     */
    HorsAVDecoderModeAuto          = 0,
    /**
     * Force software decoder
     */
    HorsAVDecoderModeSoftware      = 1,
    /**
     * Force hardware decoder
     */
    HorsAVDecoderModeHardware      = 2,
    /**
     * Try hardware first, fallback to software
     */
    HorsAVDecoderModeHardwareFirst = 3
} NS_SWIFT_NAME(DecoderMode);

// MARK: - Media Info

/**
 * Media information class
 * Contains metadata about the opened media file
 */
NS_SWIFT_NAME(MediaInfo)
@interface HorsAVMediaInfo : NSObject <NSCopying>

/**
 * Media URL
 */
@property (nonatomic, readonly, copy) NSString *url;

/**
 * Container format (e.g., "mp4", "mov", "mkv")
 */
@property (nonatomic, readonly, copy) NSString *format;

/**
 * Media duration in seconds
 */
@property (nonatomic, readonly) NSTimeInterval duration;

/**
 * Video width in pixels, 0 if no video
 */
@property (nonatomic, readonly) NSInteger videoWidth;

/**
 * Video height in pixels, 0 if no video
 */
@property (nonatomic, readonly) NSInteger videoHeight;

/**
 * Video frame rate
 */
@property (nonatomic, readonly) double videoFrameRate;

/**
 * Audio sample rate in Hz
 */
@property (nonatomic, readonly) NSInteger audioSampleRate;

/**
 * Number of audio channels
 */
@property (nonatomic, readonly) NSInteger audioChannels;

/**
 * Whether media has video stream
 */
@property (nonatomic, readonly) BOOL hasVideo;

/**
 * Whether media has audio stream
 */
@property (nonatomic, readonly) BOOL hasAudio;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

// MARK: - Data Bypass Types

/**
 * Data bypass stage
 */
NS_SWIFT_NAME(DataBypassStage)
typedef NS_ENUM(NSInteger, HorsAVDataBypassStage) {
    /**
     * Data after demuxing, before decoding
     */
    HorsAVDataBypassStageDemuxed   = 0,
    /**
     * Data after decoding, raw frames
     */
    HorsAVDataBypassStageDecoded   = 1,
    /**
     * Data after processing, before rendering
     */
    HorsAVDataBypassStageProcessed = 2,
    /**
     * Data before final rendering
     */
    HorsAVDataBypassStageRendered  = 3
} NS_SWIFT_NAME(DataBypassStage);

/**
 * Data type for bypass
 */
NS_SWIFT_NAME(DataType)
typedef NS_ENUM(NSInteger, HorsAVDataType) {
    HorsAVDataTypeVideo = 0,
    HorsAVDataTypeAudio = 1
} NS_SWIFT_NAME(DataType);

// MARK: - Render Mode

/**
 * Video render mode
 */
NS_SWIFT_NAME(RenderMode)
typedef NS_ENUM(NSInteger, HorsAVRenderMode) {
    /**
     * Stretch to fill the view
     */
    HorsAVRenderModeFill     = 0,
    /**
     * Fit within the view, preserving aspect ratio
     */
    HorsAVRenderModeFit      = 1,
    /**
     * Fill the view, cropping if necessary
     */
    HorsAVRenderModeFillCrop = 2
} NS_SWIFT_NAME(RenderMode);

// MARK: - Video Frame

/**
 * Video frame data for data bypass
 */
NS_SWIFT_NAME(VideoFrame)
@interface HorsAVVideoFrame : NSObject

@property (nonatomic, readonly) CGSize size;
@property (nonatomic, readonly) int64_t timestamp;  // Milliseconds
@property (nonatomic, readonly) BOOL isKeyFrame;
@property (nonatomic, readonly) HorsAVDataBypassStage stage;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

// MARK: - Audio Frame

/**
 * Audio frame data for data bypass
 */
NS_SWIFT_NAME(AudioFrame)
@interface HorsAVAudioFrame : NSObject

@property (nonatomic, readonly) NSInteger sampleRate;
@property (nonatomic, readonly) NSInteger channels;
@property (nonatomic, readonly) int64_t timestamp;  // Milliseconds
@property (nonatomic, readonly) NSInteger samples;
@property (nonatomic, readonly) HorsAVDataBypassStage stage;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
