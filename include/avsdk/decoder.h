#pragma once
#include <memory>
#include "avsdk/error.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace avsdk {

using AVPacketPtr = std::shared_ptr<AVPacket>;
using AVFramePtr = std::shared_ptr<AVFrame>;

class IDecoder {
public:
    virtual ~IDecoder() = default;

    virtual ErrorCode Initialize(AVCodecParameters* codecpar) = 0;
    virtual AVFramePtr Decode(const AVPacketPtr& packet) = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;
};

std::unique_ptr<IDecoder> CreateFFmpegDecoder();

} // namespace avsdk
