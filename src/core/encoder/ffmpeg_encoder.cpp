#include "ffmpeg_encoder.h"
#include "avsdk/logger.h"

namespace avsdk {

FFmpegEncoder::FFmpegEncoder() = default;

FFmpegEncoder::~FFmpegEncoder() {
    Close();
}

ErrorCode FFmpegEncoder::Initialize(const EncoderConfig& config) {
    width_ = config.width;
    height_ = config.height;

    AVCodecID codec_id;
    switch (config.codec) {
        case EncoderCodec::H264:
            codec_id = AV_CODEC_ID_H264;
            break;
        case EncoderCodec::H265:
            codec_id = AV_CODEC_ID_HEVC;
            break;
        default:
            return ErrorCode::CodecNotFound;
    }

    codec_ = avcodec_find_encoder(codec_id);
    if (!codec_) {
        LOG_ERROR("Encoder", "Codec not found");
        return ErrorCode::CodecNotFound;
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        return ErrorCode::OutOfMemory;
    }

    codec_ctx_->width = config.width;
    codec_ctx_->height = config.height;
    codec_ctx_->time_base = {1, config.frame_rate};
    codec_ctx_->framerate = {config.frame_rate, 1};
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx_->bit_rate = config.bitrate;
    codec_ctx_->gop_size = config.gop_size;

    int ret = avcodec_open2(codec_ctx_, codec_, nullptr);
    if (ret < 0) {
        LOG_ERROR("Encoder", "Failed to open codec");
        return ErrorCode::CodecOpenFailed;
    }

    initialized_ = true;
    LOG_INFO("Encoder", "Initialized " + std::to_string(width_) + "x" + std::to_string(height_) + " encoder");
    return ErrorCode::OK;
}

AVPacketPtr FFmpegEncoder::Encode(const AVFramePtr& frame) {
    if (!codec_ctx_ || !frame) return nullptr;

    int ret = avcodec_send_frame(codec_ctx_, frame.get());
    if (ret < 0) {
        return nullptr;
    }

    AVPacket* packet = av_packet_alloc();
    ret = avcodec_receive_packet(codec_ctx_, packet);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_packet_free(&packet);
        return nullptr;
    }

    return AVPacketPtr(packet, [](AVPacket* p) { av_packet_free(&p); });
}

AVPacketPtr FFmpegEncoder::Flush() {
    if (!codec_ctx_) return nullptr;

    avcodec_send_frame(codec_ctx_, nullptr);

    AVPacket* packet = av_packet_alloc();
    int ret = avcodec_receive_packet(codec_ctx_, packet);

    if (ret == AVERROR_EOF) {
        av_packet_free(&packet);
        return nullptr;
    }

    return AVPacketPtr(packet, [](AVPacket* p) { av_packet_free(&p); });
}

void FFmpegEncoder::Close() {
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
    initialized_ = false;
}

std::unique_ptr<IEncoder> CreateFFmpegEncoder() {
    return std::make_unique<FFmpegEncoder>();
}

} // namespace avsdk
