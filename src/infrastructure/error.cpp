#include "avsdk/error.h"

namespace avsdk {

const char* GetErrorString(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK:
            return "OK";
        case ErrorCode::Unknown:
            return "Unknown error";
        case ErrorCode::InvalidParameter:
            return "Invalid parameter";
        case ErrorCode::OutOfMemory:
            return "Out of memory";
        case ErrorCode::NotInitialized:
            return "Not initialized";
        case ErrorCode::PlayerOpenFailed:
            return "Failed to open player";
        case ErrorCode::PlayerSeekFailed:
            return "Failed to seek";
        case ErrorCode::CodecNotFound:
            return "Codec not found";
        case ErrorCode::CodecOpenFailed:
            return "Failed to open codec";
        case ErrorCode::CodecDecodeFailed:
            return "Failed to decode";
        case ErrorCode::NetworkConnectFailed:
            return "Network connection failed";
        case ErrorCode::FileNotFound:
            return "File not found";
        case ErrorCode::FileOpenFailed:
            return "Failed to open file";
        case ErrorCode::FileInvalidFormat:
            return "Invalid file format";
        case ErrorCode::HardwareNotAvailable:
            return "Hardware acceleration not available";
        default:
            return "Unknown error";
    }
}

} // namespace avsdk
