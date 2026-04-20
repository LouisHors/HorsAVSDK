#pragma once
#include "avsdk/encoder.h"
extern "C" {
#include <libavcodec/avcodec.h>
}

namespace avsdk {

class FFmpegEncoder : public IEncoder {
public:
    FFmpegEncoder();
    ~FFmpegEncoder() override;

    ErrorCode Initialize(const EncoderConfig& config) override;
    AVPacketPtr Encode(const AVFramePtr& frame) override;
    AVPacketPtr Flush() override;
    void Close() override;

    int GetWidth() const override { return width_; }
    int GetHeight() const override { return height_; }

private:
    AVCodecContext* codec_ctx_ = nullptr;
    const AVCodec* codec_ = nullptr;

    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
};

} // namespace avsdk
