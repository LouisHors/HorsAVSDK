#pragma once
#include <memory>
#include "avsdk/error.h"

struct AVPacket;
struct AVFrame;

namespace avsdk {

using AVPacketPtr = std::shared_ptr<AVPacket>;
using AVFramePtr = std::shared_ptr<AVFrame>;

enum class CodecType {
    H264,
    H265,
    VP8,
    VP9,
    AV1
};

class IDecoder {
public:
    virtual ~IDecoder() = default;

    virtual ErrorCode Initialize(CodecType codec_type, int width, int height) = 0;
    virtual AVFramePtr Decode(const AVPacketPtr& packet) = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;
};

std::unique_ptr<IDecoder> CreateFFmpegDecoder();

} // namespace avsdk
