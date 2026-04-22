#pragma once

// HorsAVError.h
// Error handling for HorsAVSDK

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Error domain for HorsAVSDK errors
 */
NS_SWIFT_NAME(HorsAVErrorDomain)
extern NSErrorDomain _Nonnull const HorsAVErrorDomain;

/**
 * Error codes for HorsAVSDK
 */
NS_SWIFT_NAME(ErrorCode)
typedef NS_ERROR_ENUM(HorsAVErrorDomain, HorsAVErrorCode) {
    /**
     * Operation successful
     */
    HorsAVErrorCodeOK                      = 0,
    /**
     * Unknown error
     */
    HorsAVErrorCodeUnknown                 = 1,
    /**
     * Invalid parameter provided
     */
    HorsAVErrorCodeInvalidParameter        = 2,
    /**
     * Out of memory
     */
    HorsAVErrorCodeOutOfMemory             = 4,
    /**
     * Player not initialized
     */
    HorsAVErrorCodeNotInitialized          = 6,

    // Player errors (100-199)
    /**
     * Failed to open media file
     */
    HorsAVErrorCodePlayerOpenFailed        = 103,
    /**
     * Seek operation failed
     */
    HorsAVErrorCodePlayerSeekFailed        = 104,

    // Codec errors (200-299)
    /**
     * Codec not found
     */
    HorsAVErrorCodeCodecNotFound           = 200,
    /**
     * Failed to open codec
     */
    HorsAVErrorCodeCodecOpenFailed         = 201,
    /**
     * Decoding failed
     */
    HorsAVErrorCodeCodecDecodeFailed       = 202,

    // Network errors (300-399)
    /**
     * Network connection failed
     */
    HorsAVErrorCodeNetworkConnectFailed    = 300,
    /**
     * Network timeout
     */
    HorsAVErrorCodeNetworkTimeout          = 301,
    /**
     * HTTP error response
     */
    HorsAVErrorCodeNetworkHTTPError        = 302,
    /**
     * Invalid URL
     */
    HorsAVErrorCodeNetworkInvalidURL       = 303,

    // File errors (400-499)
    /**
     * File not found
     */
    HorsAVErrorCodeFileNotFound            = 400,
    /**
     * Failed to open file
     */
    HorsAVErrorCodeFileOpenFailed          = 401,
    /**
     * Invalid file format
     */
    HorsAVErrorCodeFileInvalidFormat       = 404,

    // Hardware errors (500-599)
    /**
     * Hardware decoder/encoder not available
     */
    HorsAVErrorCodeHardwareNotAvailable    = 500,
    /**
     * Hardware operation failed
     */
    HorsAVErrorCodeHardwareFailed          = 501
} NS_SWIFT_NAME(ErrorCode);

/**
 * Returns a localized description for an error code
 * @param code The error code
 * @return Localized description string
 */
NS_SWIFT_NAME(GetErrorDescription(_:))
NSString * _Nullable HorsAVGetErrorDescription(HorsAVErrorCode code);

NS_ASSUME_NONNULL_END
