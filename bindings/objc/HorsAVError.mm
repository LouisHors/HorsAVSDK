#import "HorsAVError.h"

// MARK: - Error Domain

NSErrorDomain _Nonnull const HorsAVErrorDomain = @"com.horsav.sdk";

// MARK: - Error Descriptions

NS_SWIFT_NAME(GetErrorDescription(_:))
NSString * _Nullable HorsAVGetErrorDescription(HorsAVErrorCode code) {
    switch (code) {
        case HorsAVErrorCodeOK:
            return @"Success";
        case HorsAVErrorCodeUnknown:
            return @"Unknown error";
        case HorsAVErrorCodeInvalidParameter:
            return @"Invalid parameter";
        case HorsAVErrorCodeOutOfMemory:
            return @"Out of memory";
        case HorsAVErrorCodeNotInitialized:
            return @"Not initialized";
        case HorsAVErrorCodePlayerOpenFailed:
            return @"Failed to open media";
        case HorsAVErrorCodePlayerSeekFailed:
            return @"Seek operation failed";
        case HorsAVErrorCodeCodecNotFound:
            return @"Codec not found";
        case HorsAVErrorCodeCodecOpenFailed:
            return @"Failed to open codec";
        case HorsAVErrorCodeCodecDecodeFailed:
            return @"Decoding failed";
        case HorsAVErrorCodeNetworkConnectFailed:
            return @"Network connection failed";
        case HorsAVErrorCodeNetworkTimeout:
            return @"Network timeout";
        case HorsAVErrorCodeNetworkHTTPError:
            return @"HTTP error";
        case HorsAVErrorCodeNetworkInvalidURL:
            return @"Invalid URL";
        case HorsAVErrorCodeFileNotFound:
            return @"File not found";
        case HorsAVErrorCodeFileOpenFailed:
            return @"Failed to open file";
        case HorsAVErrorCodeFileInvalidFormat:
            return @"Invalid file format";
        case HorsAVErrorCodeHardwareNotAvailable:
            return @"Hardware not available";
        case HorsAVErrorCodeHardwareFailed:
            return @"Hardware operation failed";
        default:
            return nil;
    }
}
