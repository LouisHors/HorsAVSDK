#pragma once

// HorsAVLogger.h
// Logging system for HorsAVSDK

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Log level enumeration
 */
NS_SWIFT_NAME(LogLevel)
typedef NS_ENUM(NSInteger, HorsAVLogLevel) {
    HorsAVLogLevelTrace   = 0,
    HorsAVLogLevelDebug   = 1,
    HorsAVLogLevelInfo    = 2,
    HorsAVLogLevelWarning = 3,
    HorsAVLogLevelError   = 4,
    HorsAVLogLevelOff     = 5
} NS_SWIFT_NAME(LogLevel);

/**
 * Logger class for HorsAVSDK
 *
 * Configure global logging behavior using this class.
 */
NS_SWIFT_NAME(Logger)
@interface HorsAVLogger : NSObject

/**
 * Set the global log level
 * @param level The minimum log level to output
 */
+ (void)setLevel:(HorsAVLogLevel)level NS_SWIFT_NAME(setLevel(_:));

/**
 * Get the current global log level
 */
+ (HorsAVLogLevel)currentLevel;

/**
 * Enable or disable console logging
 * @param enable YES to enable console output
 */
+ (void)enableConsoleLogging:(BOOL)enable NS_SWIFT_NAME(enableConsoleLogging(_:));

/**
 * Set a custom log callback
 * @param callback Block called for each log message
 */
+ (void)setCallback:(nullable void (^)(HorsAVLogLevel level, NSString *message))callback
    NS_SWIFT_NAME(setCallback(_:));

/**
 * Clear the custom log callback
 */
+ (void)clearCallback;

@end

NS_ASSUME_NONNULL_END
