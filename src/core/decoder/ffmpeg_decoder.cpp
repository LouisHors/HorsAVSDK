#include "ffmpeg_decoder.h"
#include "avsdk/logger.h"

namespace avsdk {

FFmpegDecoder::FFmpegDecoder() = default;

FFmpegDecoder::~FFmpegDecoder() {
    Close();
}

ErrorCode FFmpegDecoder::Initialize(AVCodecParameters* codecpar) {
    if (!codecpar) {
        return ErrorCode::InvalidParameter;
    }

    // Find decoder based on codec_id from stream
    codec_ = avcodec_find_decoder(codecpar->codec_id);
    if (!codec_) {
        LOG_ERROR("Decoder", "Codec not found for codec_id: " + std::to_string(codecpar->codec_id));
        return ErrorCode::CodecNotFound;
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        return ErrorCode::OutOfMemory;
    }

    // Copy codec parameters from demuxer
    int ret = avcodec_parameters_to_context(codec_ctx_, codecpar);
    if (ret < 0) {
        LOG_ERROR("Decoder", "Failed to copy codec parameters");
        avcodec_free_context(&codec_ctx_);
        return ErrorCode::CodecOpenFailed;
    }

    // Enable multi-threading
    codec_ctx_->thread_count = 4;
    codec_ctx_->thread_type = FF_THREAD_FRAME;

    ret = avcodec_open2(codec_ctx_, codec_, nullptr);
    if (ret < 0) {
        LOG_ERROR("Decoder", "Failed to open codec");
        return ErrorCode::CodecOpenFailed;
    }

    LOG_INFO("Decoder", "Initialized decoder: " + std::string(codec_->name) +
             " " + std::to_string(codec_ctx_->width) + "x" + std::to_string(codec_ctx_->height));
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
