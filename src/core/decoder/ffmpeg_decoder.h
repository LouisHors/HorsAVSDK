#pragma once
#include "avsdk/decoder.h"

namespace avsdk {

class FFmpegDecoder : public IDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    ErrorCode Initialize(AVCodecParameters* codecpar) override;
    AVFramePtr Decode(const AVPacketPtr& packet) override;
    void Flush() override;
    void Close() override;

private:
    AVCodecContext* codec_ctx_ = nullptr;
    const AVCodec* codec_ = nullptr;
};

} // namespace avsdk
