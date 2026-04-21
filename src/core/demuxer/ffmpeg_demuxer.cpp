#include "ffmpeg_demuxer.h"
#include "avsdk/logger.h"
#include <cstring>
#include <sys/stat.h>

extern "C" {
#include <libavformat/avformat.h>
}

namespace avsdk {

FFmpegDemuxer::FFmpegDemuxer() = default;

FFmpegDemuxer::~FFmpegDemuxer() {
    Close();
}

ErrorCode FFmpegDemuxer::Open(const std::string& url) {
    // Check if file exists for file:// URLs or local paths
    struct stat st;
    if (url.find("://") == std::string::npos || url.find("file://") == 0) {
        std::string file_path = url;
        if (url.find("file://") == 0) {
            file_path = url.substr(7); // Remove "file://" prefix
        }
        if (stat(file_path.c_str(), &st) != 0) {
            LOG_ERROR("Demuxer", "File not found: " + url);
            return ErrorCode::FileNotFound;
        }
    }

    format_ctx_ = avformat_alloc_context();

    int ret = avformat_open_input(&format_ctx_, url.c_str(), nullptr, nullptr);
    if (ret < 0) {
        LOG_ERROR("Demuxer", "Failed to open input: " + url);
        return ErrorCode::FileOpenFailed;
    }

    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if (ret < 0) {
        LOG_ERROR("Demuxer", "Failed to find stream info");
        return ErrorCode::FileInvalidFormat;
    }

    // Find video and audio streams
    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        AVStream* stream = format_ctx_->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            media_info_.has_video = true;
            media_info_.video_width = stream->codecpar->width;
            media_info_.video_height = stream->codecpar->height;
            media_info_.video_framerate = av_q2d(stream->avg_frame_rate);
            LOG_INFO("Demuxer", "Video stream index: " + std::to_string(i) +
                     " codec_id: " + std::to_string(stream->codecpar->codec_id));
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index_ = i;
            media_info_.has_audio = true;
            media_info_.audio_sample_rate = stream->codecpar->sample_rate;
            media_info_.audio_channels = stream->codecpar->ch_layout.nb_channels;
            LOG_INFO("Demuxer", "Audio stream index: " + std::to_string(i));
        }
    }

    media_info_.url = url;
    media_info_.format = format_ctx_->iformat->name;
    media_info_.duration_ms = format_ctx_->duration * 1000 / AV_TIME_BASE;

    LOG_INFO("Demuxer", "Opened: " + url + ", duration: " + std::to_string(media_info_.duration_ms) + "ms");

    return ErrorCode::OK;
}

void FFmpegDemuxer::Close() {
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
    }
}

AVPacketPtr FFmpegDemuxer::ReadPacket() {
    if (!format_ctx_) return nullptr;

    AVPacket* packet = av_packet_alloc();
    int ret = av_read_frame(format_ctx_, packet);

    if (ret < 0) {
        av_packet_free(&packet);
        return nullptr;
    }

    return AVPacketPtr(packet, [](AVPacket* p) { av_packet_free(&p); });
}

ErrorCode FFmpegDemuxer::Seek(Timestamp position_ms) {
    if (!format_ctx_) return ErrorCode::NotInitialized;

    int64_t target = position_ms * AV_TIME_BASE / 1000;
    int ret = av_seek_frame(format_ctx_, -1, target, AVSEEK_FLAG_BACKWARD);

    return (ret >= 0) ? ErrorCode::OK : ErrorCode::PlayerSeekFailed;
}

MediaInfo FFmpegDemuxer::GetMediaInfo() const {
    return media_info_;
}

AVCodecParameters* FFmpegDemuxer::GetVideoCodecParameters() const {
    if (!format_ctx_ || video_stream_index_ < 0) {
        return nullptr;
    }
    return format_ctx_->streams[video_stream_index_]->codecpar;
}

AVCodecParameters* FFmpegDemuxer::GetAudioCodecParameters() const {
    if (!format_ctx_ || audio_stream_index_ < 0) {
        return nullptr;
    }
    return format_ctx_->streams[audio_stream_index_]->codecpar;
}

double FFmpegDemuxer::GetVideoTimebase() const {
    if (!format_ctx_ || video_stream_index_ < 0) {
        return 0.0;
    }
    AVStream* stream = format_ctx_->streams[video_stream_index_];
    return av_q2d(stream->time_base);
}

double FFmpegDemuxer::GetAudioTimebase() const {
    if (!format_ctx_ || audio_stream_index_ < 0) {
        return 0.0;
    }
    AVStream* stream = format_ctx_->streams[audio_stream_index_];
    return av_q2d(stream->time_base);
}

std::unique_ptr<IDemuxer> CreateFFmpegDemuxer() {
    return std::make_unique<FFmpegDemuxer>();
}

} // namespace avsdk
