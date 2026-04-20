#pragma once
#include "avsdk/decoder.h"
extern "C" {
#include <libavcodec/avcodec.h>
}

namespace avsdk {

class FFmpegDecoder : public IDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    ErrorCode Initialize(CodecType codec_type, int width, int height) override;
    AVFramePtr Decode(const AVPacketPtr& packet) override;
    void Flush() override;
    void Close() override;

private:
    AVCodecContext* codec_ctx_ = nullptr;
    const AVCodec* codec_ = nullptr;
};

} // namespace avsdk
