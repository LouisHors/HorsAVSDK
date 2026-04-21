#include "video_recorder.h"
#include "avsdk/logger.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/avstring.h>
#include <libavutil/timestamp.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
}

#include <algorithm>
#include <chrono>
#include <cstring>

namespace avsdk {

VideoRecorder::VideoRecorder(const RecorderConfig& config)
    : config_(config),
      formatCtx_(nullptr),
      videoCodecCtx_(nullptr),
      audioCodecCtx_(nullptr),
      videoStream_(nullptr),
      audioStream_(nullptr),
      swsCtx_(nullptr),
      swrCtx_(nullptr),
      isRecording_(false),
      startTimeMs_(0),
      videoFramesWritten_(0),
      audioFramesWritten_(0),
      videoPts_(0),
      audioPts_(0) {
}

VideoRecorder::~VideoRecorder() {
    Stop();
    Cleanup();
}

int VideoRecorder::Start(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isRecording_) {
        LOG_ERROR("VideoRecorder", "Already recording");
        return -1;
    }

    // Detect format from path
    std::string format = GetOutputFormatFromPath(path);
    if (format.empty()) {
        LOG_ERROR("VideoRecorder", "Unknown output format: " + path);
        return -1;
    }

    // Allocate output context
    int ret = avformat_alloc_output_context2(&formatCtx_, nullptr, format.c_str(), path.c_str());
    if (ret < 0 || !formatCtx_) {
        LOG_ERROR("VideoRecorder", "Failed to allocate output context");
        return -1;
    }

    // Initialize video stream
    ret = InitializeVideo();
    if (ret < 0) {
        Cleanup();
        return ret;
    }

    // Initialize audio stream
    ret = InitializeAudio();
    if (ret < 0) {
        Cleanup();
        return ret;
    }

    // Open output file
    if (!(formatCtx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&formatCtx_->pb, path.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOG_ERROR("VideoRecorder", "Failed to open output file: " + path);
            Cleanup();
            return -1;
        }
    }

    // Write header
    ret = avformat_write_header(formatCtx_, nullptr);
    if (ret < 0) {
        LOG_ERROR("VideoRecorder", "Failed to write header");
        Cleanup();
        return -1;
    }

    // Initialize conversion contexts
    swsCtx_ = sws_getContext(
        config_.videoWidth, config_.videoHeight, AV_PIX_FMT_YUV420P,
        config_.videoWidth, config_.videoHeight, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!swsCtx_) {
        LOG_ERROR("VideoRecorder", "Failed to create sws context");
        Cleanup();
        return -1;
    }

    // Initialize audio resampler
    AVChannelLayout in_ch_layout, out_ch_layout;
    av_channel_layout_default(&out_ch_layout, config_.audioChannels);
    av_channel_layout_default(&in_ch_layout, config_.audioChannels);

    ret = swr_alloc_set_opts2(&swrCtx_,
        &out_ch_layout, AV_SAMPLE_FMT_FLTP, config_.audioSampleRate,
        &in_ch_layout, AV_SAMPLE_FMT_FLTP, config_.audioSampleRate,
        0, nullptr);

    if (ret < 0 || !swrCtx_) {
        LOG_ERROR("VideoRecorder", "Failed to create swr context");
        Cleanup();
        return -1;
    }

    ret = swr_init(swrCtx_);
    if (ret < 0) {
        LOG_ERROR("VideoRecorder", "Failed to initialize swr context");
        Cleanup();
        return -1;
    }

    // Reset state
    isRecording_ = true;
    startTimeMs_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    videoFramesWritten_ = 0;
    audioFramesWritten_ = 0;
    videoPts_ = 0;
    audioPts_ = 0;

    LOG_INFO("VideoRecorder", "Started recording to: " + path);
    return 0;
}

void VideoRecorder::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isRecording_) {
        return;
    }

    // Write trailer
    if (formatCtx_) {
        av_write_trailer(formatCtx_);
    }

    // Close file
    if (formatCtx_ && formatCtx_->pb && !(formatCtx_->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&formatCtx_->pb);
    }

    // Calculate duration before clearing state
    int64_t duration = 0;
    if (isRecording_) {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        duration = now - startTimeMs_;
    }

    isRecording_ = false;

    LOG_INFO("VideoRecorder", "Stopped recording. Duration: " + std::to_string(duration) +
             "ms, Video frames: " + std::to_string(videoFramesWritten_) +
             ", Audio frames: " + std::to_string(audioFramesWritten_));
}

bool VideoRecorder::IsRecording() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isRecording_;
}

int64_t VideoRecorder::GetRecordingDurationMs() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isRecording_) {
        return 0;
    }

    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    return now - startTimeMs_;
}

void VideoRecorder::GetStats(int64_t& videoFrames, int64_t& audioFrames) const {
    std::lock_guard<std::mutex> lock(mutex_);
    videoFrames = videoFramesWritten_;
    audioFrames = audioFramesWritten_;
}

// IDataBypass interface methods
void VideoRecorder::OnRawPacket(const EncodedPacket& /*packet*/) {
    // Not used - we record decoded frames
}

void VideoRecorder::OnDecodedVideoFrame(const VideoFrame& frame) {
    if (IsRecording()) {
        WriteVideoFrame(frame);
    }
}

void VideoRecorder::OnDecodedAudioFrame(const AudioFrame& frame) {
    if (IsRecording()) {
        WriteAudioFrame(frame);
    }
}

void VideoRecorder::OnPreRenderVideoFrame(VideoFrame& /*frame*/) {
    // We record decoded frames, not pre-render
}

void VideoRecorder::OnPreRenderAudioFrame(AudioFrame& /*frame*/) {
    // We record decoded frames, not pre-render
}

void VideoRecorder::OnEncodedPacket(const EncodedPacket& /*packet*/) {
    // Not used - we record decoded frames
}

// Static Create method
std::shared_ptr<VideoRecorder> VideoRecorder::Create(const RecorderConfig& config) {
    return std::make_shared<VideoRecorder>(config);
}

int VideoRecorder::InitializeVideo() {
    // Find video encoder
    const AVCodec* codec = nullptr;
    if (config_.videoCodec == "libx264") {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    } else if (config_.videoCodec == "libx265") {
        codec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
    } else {
        codec = avcodec_find_encoder_by_name(config_.videoCodec.c_str());
    }

    if (!codec) {
        LOG_ERROR("VideoRecorder", "Video encoder not found: " + config_.videoCodec);
        return -1;
    }

    // Allocate codec context
    videoCodecCtx_ = avcodec_alloc_context3(codec);
    if (!videoCodecCtx_) {
        LOG_ERROR("VideoRecorder", "Failed to allocate video codec context");
        return -1;
    }

    // Set codec parameters
    videoCodecCtx_->width = config_.videoWidth;
    videoCodecCtx_->height = config_.videoHeight;
    videoCodecCtx_->bit_rate = config_.videoBitrate;
    videoCodecCtx_->time_base = {1, config_.frameRate};
    videoCodecCtx_->framerate = {config_.frameRate, 1};
    videoCodecCtx_->gop_size = config_.frameRate;  // 1 second GOP
    videoCodecCtx_->max_b_frames = 2;
    videoCodecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;

    // Set codec-specific options
    if (codec->id == AV_CODEC_ID_H264 || codec->id == AV_CODEC_ID_HEVC) {
        av_opt_set(videoCodecCtx_->priv_data, "preset", "fast", 0);
        av_opt_set(videoCodecCtx_->priv_data, "tune", "zerolatency", 0);
    }

    // Open codec
    int ret = avcodec_open2(videoCodecCtx_, codec, nullptr);
    if (ret < 0) {
        LOG_ERROR("VideoRecorder", "Failed to open video codec");
        avcodec_free_context(&videoCodecCtx_);
        return -1;
    }

    // Create stream
    videoStream_ = avformat_new_stream(formatCtx_, nullptr);
    if (!videoStream_) {
        LOG_ERROR("VideoRecorder", "Failed to create video stream");
        avcodec_free_context(&videoCodecCtx_);
        return -1;
    }

    // Copy codec params to stream
    ret = avcodec_parameters_from_context(videoStream_->codecpar, videoCodecCtx_);
    if (ret < 0) {
        LOG_ERROR("VideoRecorder", "Failed to copy video codec params");
        avcodec_free_context(&videoCodecCtx_);
        return -1;
    }

    videoStream_->time_base = videoCodecCtx_->time_base;

    LOG_INFO("VideoRecorder", "Video stream initialized: " +
             std::to_string(config_.videoWidth) + "x" + std::to_string(config_.videoHeight) +
             " @ " + std::to_string(config_.frameRate) + "fps");

    return 0;
}

int VideoRecorder::InitializeAudio() {
    // Find audio encoder
    const AVCodec* codec = nullptr;
    if (config_.audioCodec == "aac") {
        codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    } else if (config_.audioCodec == "mp3") {
        codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
    } else {
        codec = avcodec_find_encoder_by_name(config_.audioCodec.c_str());
    }

    if (!codec) {
        LOG_ERROR("VideoRecorder", "Audio encoder not found: " + config_.audioCodec);
        return -1;
    }

    // Allocate codec context
    audioCodecCtx_ = avcodec_alloc_context3(codec);
    if (!audioCodecCtx_) {
        LOG_ERROR("VideoRecorder", "Failed to allocate audio codec context");
        return -1;
    }

    // Set codec parameters
    audioCodecCtx_->sample_rate = config_.audioSampleRate;
#if LIBAVCODEC_VERSION_MAJOR < 61
    audioCodecCtx_->channels = config_.audioChannels;
    audioCodecCtx_->channel_layout = av_get_default_channel_layout(config_.audioChannels);
#else
    av_channel_layout_default(&audioCodecCtx_->ch_layout, config_.audioChannels);
#endif
    audioCodecCtx_->bit_rate = config_.audioBitrate;
    audioCodecCtx_->sample_fmt = AV_SAMPLE_FMT_FLTP;

    // Open codec
    int ret = avcodec_open2(audioCodecCtx_, codec, nullptr);
    if (ret < 0) {
        LOG_ERROR("VideoRecorder", "Failed to open audio codec");
        avcodec_free_context(&audioCodecCtx_);
        return -1;
    }

    // Create stream
    audioStream_ = avformat_new_stream(formatCtx_, nullptr);
    if (!audioStream_) {
        LOG_ERROR("VideoRecorder", "Failed to create audio stream");
        avcodec_free_context(&audioCodecCtx_);
        return -1;
    }

    // Copy codec params to stream
    ret = avcodec_parameters_from_context(audioStream_->codecpar, audioCodecCtx_);
    if (ret < 0) {
        LOG_ERROR("VideoRecorder", "Failed to copy audio codec params");
        avcodec_free_context(&audioCodecCtx_);
        return -1;
    }

    audioStream_->time_base = {1, config_.audioSampleRate};

    LOG_INFO("VideoRecorder", "Audio stream initialized: " +
             std::to_string(config_.audioSampleRate) + "Hz, " +
             std::to_string(config_.audioChannels) + " channels");

    return 0;
}

int VideoRecorder::WriteVideoFrame(const VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isRecording_ || !videoCodecCtx_) {
        return -1;
    }

    // Allocate AVFrame for encoding
    AVFrame* avFrame = av_frame_alloc();
    if (!avFrame) {
        LOG_ERROR("VideoRecorder", "Failed to allocate video frame");
        return -1;
    }

    avFrame->width = config_.videoWidth;
    avFrame->height = config_.videoHeight;
    avFrame->format = AV_PIX_FMT_YUV420P;
    avFrame->pts = videoPts_++;

    int ret = av_frame_get_buffer(avFrame, 0);
    if (ret < 0) {
        LOG_ERROR("VideoRecorder", "Failed to allocate video frame buffer");
        av_frame_free(&avFrame);
        return -1;
    }

    // Copy/convert frame data
    // For now, assume input is YUV420P, same dimensions
    // TODO: Add proper format conversion using swsCtx_
    if (frame.format == AV_PIX_FMT_YUV420P &&
        frame.resolution.width == config_.videoWidth &&
        frame.resolution.height == config_.videoHeight) {

        for (int i = 0; i < 3; i++) {
            int h = (i == 0) ? config_.videoHeight : config_.videoHeight / 2;
            for (int y = 0; y < h; y++) {
                memcpy(avFrame->data[i] + y * avFrame->linesize[i],
                       frame.data[i] + y * frame.linesize[i],
                       std::min(avFrame->linesize[i], frame.linesize[i]));
            }
        }
    } else {
        // Need format conversion
        AVPixelFormat srcFormat = static_cast<AVPixelFormat>(frame.format);
        struct SwsContext* sws_ctx = sws_getContext(
            frame.resolution.width, frame.resolution.height, srcFormat,
            config_.videoWidth, config_.videoHeight, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!sws_ctx) {
            LOG_ERROR("VideoRecorder", "Failed to create conversion context");
            av_frame_free(&avFrame);
            return -1;
        }

        sws_scale(sws_ctx, frame.data, frame.linesize, 0, frame.resolution.height,
                  avFrame->data, avFrame->linesize);
        sws_freeContext(sws_ctx);
    }

    // Encode frame
    ret = avcodec_send_frame(videoCodecCtx_, avFrame);
    av_frame_free(&avFrame);

    if (ret < 0) {
        LOG_ERROR("VideoRecorder", "Failed to send frame to encoder");
        return -1;
    }

    // Receive encoded packets
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("VideoRecorder", "Failed to allocate packet");
        return -1;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(videoCodecCtx_, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            LOG_ERROR("VideoRecorder", "Failed to receive packet from encoder");
            av_packet_free(&packet);
            return -1;
        }

        // Rescale timestamp
        av_packet_rescale_ts(packet, videoCodecCtx_->time_base, videoStream_->time_base);
        packet->stream_index = videoStream_->index;

        // Write packet
        ret = av_interleaved_write_frame(formatCtx_, packet);
        if (ret < 0) {
            LOG_ERROR("VideoRecorder", "Failed to write video packet");
            av_packet_free(&packet);
            return -1;
        }

        av_packet_unref(packet);
        videoFramesWritten_++;
    }

    av_packet_free(&packet);
    return 0;
}

int VideoRecorder::WriteAudioFrame(const AudioFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isRecording_ || !audioCodecCtx_) {
        return -1;
    }

    // Allocate AVFrame for encoding
    AVFrame* avFrame = av_frame_alloc();
    if (!avFrame) {
        LOG_ERROR("VideoRecorder", "Failed to allocate audio frame");
        return -1;
    }

    avFrame->nb_samples = frame.samples;
    avFrame->sample_rate = config_.audioSampleRate;
#if LIBAVUTIL_VERSION_MAJOR < 59
    avFrame->channels = config_.audioChannels;
    avFrame->channel_layout = av_get_default_channel_layout(config_.audioChannels);
#else
    av_channel_layout_default(&avFrame->ch_layout, config_.audioChannels);
#endif
    avFrame->format = AV_SAMPLE_FMT_FLTP;
    avFrame->pts = audioPts_;
    audioPts_ += frame.samples;

    int ret = av_frame_get_buffer(avFrame, 0);
    if (ret < 0) {
        LOG_ERROR("VideoRecorder", "Failed to allocate audio frame buffer");
        av_frame_free(&avFrame);
        return -1;
    }

    // Copy/convert audio data
    // For now, assume input is planar float with same sample rate/channels
    // TODO: Add proper format conversion using swrCtx_
    if (frame.format == AV_SAMPLE_FMT_FLTP &&
        frame.sampleRate == config_.audioSampleRate &&
        frame.channels == config_.audioChannels) {

        int sampleSize = sizeof(float);
        for (int ch = 0; ch < config_.audioChannels; ch++) {
            memcpy(avFrame->data[ch], frame.data + ch * frame.samples * sampleSize,
                   frame.samples * sampleSize);
        }
    } else {
        // Need format conversion - simplified for now
        // Real implementation would use swrCtx_
        LOG_WARNING("VideoRecorder", "Audio format conversion not implemented, skipping frame");
        av_frame_free(&avFrame);
        return -1;
    }

    // Encode frame
    ret = avcodec_send_frame(audioCodecCtx_, avFrame);
    av_frame_free(&avFrame);

    if (ret < 0) {
        LOG_ERROR("VideoRecorder", "Failed to send audio frame to encoder");
        return -1;
    }

    // Receive encoded packets
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("VideoRecorder", "Failed to allocate audio packet");
        return -1;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(audioCodecCtx_, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            LOG_ERROR("VideoRecorder", "Failed to receive audio packet from encoder");
            av_packet_free(&packet);
            return -1;
        }

        // Rescale timestamp
        av_packet_rescale_ts(packet, audioCodecCtx_->time_base, audioStream_->time_base);
        packet->stream_index = audioStream_->index;

        // Write packet
        ret = av_interleaved_write_frame(formatCtx_, packet);
        if (ret < 0) {
            LOG_ERROR("VideoRecorder", "Failed to write audio packet");
            av_packet_free(&packet);
            return -1;
        }

        av_packet_unref(packet);
        audioFramesWritten_++;
    }

    av_packet_free(&packet);
    return 0;
}

void VideoRecorder::Cleanup() {
    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }

    if (swrCtx_) {
        swr_free(&swrCtx_);
        swrCtx_ = nullptr;
    }

    if (videoCodecCtx_) {
        avcodec_free_context(&videoCodecCtx_);
        videoCodecCtx_ = nullptr;
    }

    if (audioCodecCtx_) {
        avcodec_free_context(&audioCodecCtx_);
        audioCodecCtx_ = nullptr;
    }

    if (formatCtx_) {
        avformat_free_context(formatCtx_);
        formatCtx_ = nullptr;
    }

    videoStream_ = nullptr;
    audioStream_ = nullptr;
}

std::string VideoRecorder::GetOutputFormatFromPath(const std::string& path) const {
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "";
    }

    std::string ext = path.substr(dotPos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".mp4") {
        return "mp4";
    } else if (ext == ".mov") {
        return "mov";
    } else if (ext == ".mkv") {
        return "matroska";
    } else if (ext == ".avi") {
        return "avi";
    } else if (ext == ".flv") {
        return "flv";
    }

    return "";
}

// Factory function
std::shared_ptr<IVideoRecorder> CreateVideoRecorder(const RecorderConfig& config) {
    return VideoRecorder::Create(config);
}

} // namespace avsdk
