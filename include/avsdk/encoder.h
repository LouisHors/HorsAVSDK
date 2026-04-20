#pragma once
#include <memory>
#include "avsdk/error.h"
#include "avsdk/types.h"

struct AVFrame;
struct AVPacket;

namespace avsdk {

using AVFramePtr = std::shared_ptr<AVFrame>;
using AVPacketPtr = std::shared_ptr<AVPacket>;

enum class EncoderCodec {
    H264,
    H265,
    VP8,
    VP9
};

struct EncoderConfig {
    EncoderCodec codec = EncoderCodec::H264;
    int width = 1280;
    int height = 720;
    int bitrate = 2000000;
    int frame_rate = 30;
    int gop_size = 30;
    int profile = 0;
};

class IEncoder {
public:
    virtual ~IEncoder() = default;

    virtual ErrorCode Initialize(const EncoderConfig& config) = 0;
    virtual AVPacketPtr Encode(const AVFramePtr& frame) = 0;
    virtual AVPacketPtr Flush() = 0;
    virtual void Close() = 0;

    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
};

std::unique_ptr<IEncoder> CreateFFmpegEncoder();

} // namespace avsdk
