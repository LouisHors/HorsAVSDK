#pragma once

// HorsAVSDK Umbrella Header
// Version: 1.0.0

#import <Foundation/Foundation.h>

// Core Types
#import "HorsAVTypes.h"

// Error Handling
#import "HorsAVError.h"

// Configuration
#import "HorsAVPlayerConfig.h"

// Player
#import "HorsAVPlayer.h"

// Rendering
#import "HorsAVRenderer.h"

// Data Bypass
#import "HorsAVDataBypass.h"

// Logging
#import "HorsAVLogger.h"

// Version Information
NS_ASSUME_NONNULL_BEGIN

/**
 * SDK version string (e.g., "1.0.0")
 */
FOUNDATION_EXPORT NSString * _Nonnull HorsAVSDKVersionString;

/**
 * SDK version number (e.g., 1000000 for 1.0.0)
 */
FOUNDATION_EXPORT const NSInteger HorsAVSDKVersionNumber;

NS_ASSUME_NONNULL_END
