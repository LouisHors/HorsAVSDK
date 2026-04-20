#pragma once
#include <cstdint>

namespace avsdk {

enum class ErrorCode : int32_t {
    OK = 0,
    Unknown = 1,
    InvalidParameter = 2,
    OutOfMemory = 4,
    NotInitialized = 6,

    PlayerOpenFailed = 103,
    PlayerSeekFailed = 104,

    CodecNotFound = 200,
    CodecOpenFailed = 201,
    CodecDecodeFailed = 202,

    NetworkConnectFailed = 300,

    FileNotFound = 400,
    FileOpenFailed = 401,
    FileInvalidFormat = 404,

    HardwareNotAvailable = 500,
};

const char* GetErrorString(ErrorCode code);

} // namespace avsdk
