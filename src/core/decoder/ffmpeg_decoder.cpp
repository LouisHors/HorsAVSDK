#include "ffmpeg_decoder.h"
#include "avsdk/logger.h"

namespace avsdk {

FFmpegDecoder::FFmpegDecoder() = default;

FFmpegDecoder::~FFmpegDecoder() {
    Close();
}

ErrorCode FFmpegDecoder::Initialize(CodecType codec_type, int width, int height) {
    AVCodecID codec_id;
    switch (codec_type) {
        case CodecType::H264:
            codec_id = AV_CODEC_ID_H264;
            break;
        case CodecType::H265:
            codec_id = AV_CODEC_ID_HEVC;
            break;
        case CodecType::VP8:
            codec_id = AV_CODEC_ID_VP8;
            break;
        case CodecType::VP9:
            codec_id = AV_CODEC_ID_VP9;
            break;
        case CodecType::AV1:
            codec_id = AV_CODEC_ID_AV1;
            break;
        default:
            return ErrorCode::CodecNotFound;
    }

    codec_ = avcodec_find_decoder(codec_id);
    if (!codec_) {
        LOG_ERROR("Decoder", "Codec not found");
        return ErrorCode::CodecNotFound;
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        return ErrorCode::OutOfMemory;
    }

    codec_ctx_->width = width;
    codec_ctx_->height = height;

    int ret = avcodec_open2(codec_ctx_, codec_, nullptr);
    if (ret < 0) {
        LOG_ERROR("Decoder", "Failed to open codec");
        return ErrorCode::CodecOpenFailed;
    }

    LOG_INFO("Decoder", "Initialized decoder: " + std::string(codec_->name));
    return ErrorCode::OK;
}

AVFramePtr FFmpegDecoder::Decode(const AVPacketPtr& packet) {
    if (!codec_ctx_ || !packet) return nullptr;

    int ret = avcodec_send_packet(codec_ctx_, packet.get());
    if (ret < 0) {
        return nullptr;
    }

    AVFrame* frame = av_frame_alloc();
    ret = avcodec_receive_frame(codec_ctx_, frame);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_frame_free(&frame);
        return nullptr;
    }

    return AVFramePtr(frame, [](AVFrame* f) { av_frame_free(&f); });
}

void FFmpegDecoder::Flush() {
    if (codec_ctx_) {
        avcodec_flush_buffers(codec_ctx_);
    }
}

void FFmpegDecoder::Close() {
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
}

std::unique_ptr<IDecoder> CreateFFmpegDecoder() {
    return std::make_unique<FFmpegDecoder>();
}

} // namespace avsdk
