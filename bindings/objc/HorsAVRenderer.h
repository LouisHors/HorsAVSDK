#pragma once

// HorsAVRenderer.h
// Rendering interfaces for HorsAVSDK

#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>
#import "HorsAVTypes.h"

NS_ASSUME_NONNULL_BEGIN

// MARK: - Player Renderer

/**
 * Video renderer for HorsAVPlayer
 *
 * This class wraps the platform-specific video rendering implementation.
 * Use it to configure rendering options and set custom overlays.
 */
NS_SWIFT_NAME(PlayerRenderer)
@interface HorsAVPlayerRenderer : NSObject

/**
 * Initialize with a view
 * @param view The NSView to render to. SDK will create and manage MTKView internally.
 */
- (instancetype)initWithView:(NSView *)view NS_SWIFT_NAME(init(view:)) NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

/**
 * The render mode. Default is Fit.
 */
@property (nonatomic, readwrite) HorsAVRenderMode renderMode;

/**
 * Whether to enable HDR rendering. Default is YES if display supports HDR.
 */
@property (nonatomic, readwrite) BOOL enableHDR;

/**
 * Custom overlay layer. Set to nil to remove.
 * The overlay is rendered on top of the video.
 */
@property (nonatomic, readwrite, strong, nullable) CALayer *overlayLayer;

/**
 * Background color (shown when video doesn't fill the view)
 * Default is black.
 */
@property (nonatomic, readwrite, strong, nullable) NSColor *backgroundColor;

/**
 * Current video size in pixels
 */
@property (nonatomic, readonly) CGSize videoSize;

/**
 * Current display size in points
 */
@property (nonatomic, readonly) CGSize displaySize;

/**
 * Take a screenshot of the current frame
 * @return Screenshot as NSImage, or nil if not available
 */
- (nullable NSImage *)takeScreenshot NS_SWIFT_NAME(takeScreenshot());

@end

NS_ASSUME_NONNULL_END
