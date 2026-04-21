#include "network_demuxer.h"
#include "avsdk/logger.h"
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
}

namespace avsdk {

static constexpr size_t kAvioBufferSize = 32768;
static constexpr size_t kRingBufferSize = 8 * 1024 * 1024; // 8MB

NetworkDemuxer::NetworkDemuxer()
    : ring_buffer_(std::make_unique<RingBuffer>(kRingBufferSize)) {}

NetworkDemuxer::~NetworkDemuxer() {
    Close();
}

ErrorCode NetworkDemuxer::Open(const std::string& url) {
    current_url_ = url;

    // Allocate AVIO buffer
    avio_buffer_ = static_cast<uint8_t*>(av_malloc(kAvioBufferSize));
    if (!avio_buffer_) {
        return ErrorCode::OutOfMemory;
    }

    // Create custom IO context
    io_ctx_ = avio_alloc_context(avio_buffer_, kAvioBufferSize, 0, this,
                                  &NetworkDemuxer::ReadCallback,
                                  nullptr,
                                  &NetworkDemuxer::SeekCallback);
    if (!io_ctx_) {
        av_free(avio_buffer_);
        avio_buffer_ = nullptr;
        return ErrorCode::OutOfMemory;
    }

    // Allocate format context
    format_ctx_ = avformat_alloc_context();
    if (!format_ctx_) {
        avio_context_free(&io_ctx_);
        avio_buffer_ = nullptr;
        return ErrorCode::OutOfMemory;
    }

    format_ctx_->pb = io_ctx_;

    // Start download thread
    should_stop_ = false;
    download_thread_ = std::thread(&NetworkDemuxer::DownloadThread, this);

    // Wait for some data before probing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Probe input format
    int ret = avformat_open_input(&format_ctx_, nullptr, nullptr, nullptr);
    if (ret < 0) {
        LOG_ERROR("NetworkDemuxer", "Failed to open input: " + url);
        Close();
        return ErrorCode::FileOpenFailed;
    }

    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if (ret < 0) {
        LOG_ERROR("NetworkDemuxer", "Failed to find stream info");
        Close();
        return ErrorCode::FileInvalidFormat;
    }

    // Extract media info
    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        AVStream* stream = format_ctx_->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            media_info_.has_video = true;
            media_info_.video_width = stream->codecpar->width;
            media_info_.video_height = stream->codecpar->height;
            media_info_.video_framerate = av_q2d(stream->avg_frame_rate);
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            media_info_.has_audio = true;
            media_info_.audio_sample_rate = stream->codecpar->sample_rate;
        }
    }

    media_info_.url = url;
    media_info_.duration_ms = format_ctx_->duration * 1000 / AV_TIME_BASE;

    LOG_INFO("NetworkDemuxer", "Opened: " + url);
    return ErrorCode::OK;
}

void NetworkDemuxer::Close() {
    should_stop_ = true;

    if (download_thread_.joinable()) {
        download_thread_.join();
    }

    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
    }

    if (io_ctx_) {
        avio_context_free(&io_ctx_);
        io_ctx_ = nullptr;
        avio_buffer_ = nullptr; // Freed by avio_context_free
    }
}

AVPacketPtr NetworkDemuxer::ReadPacket() {
    if (!format_ctx_) return nullptr;

    AVPacket* packet = av_packet_alloc();
    int ret = av_read_frame(format_ctx_, packet);

    if (ret < 0) {
        av_packet_free(&packet);
        return nullptr;
    }

    return AVPacketPtr(packet, [](AVPacket* p) { av_packet_free(&p); });
}

ErrorCode NetworkDemuxer::Seek(Timestamp position_ms) {
    if (!format_ctx_) return ErrorCode::NotInitialized;

    int64_t target = position_ms * AV_TIME_BASE / 1000;
    int ret = av_seek_frame(format_ctx_, -1, target, AVSEEK_FLAG_BACKWARD);

    return (ret >= 0) ? ErrorCode::OK : ErrorCode::PlayerSeekFailed;
}

MediaInfo NetworkDemuxer::GetMediaInfo() const {
    return media_info_;
}

AVCodecParameters* NetworkDemuxer::GetVideoCodecParameters() const {
    if (!format_ctx_ || video_stream_index_ < 0) {
        return nullptr;
    }
    return format_ctx_->streams[video_stream_index_]->codecpar;
}

void NetworkDemuxer::SetBufferSize(size_t size) {
    // TODO: Implement dynamic buffer resize
}

int64_t NetworkDemuxer::GetBufferedDuration() const {
    return buffered_duration_;
}

bool NetworkDemuxer::IsBuffering() const {
    return is_buffering_.load();
}

void NetworkDemuxer::DownloadThread() {
    HttpRequest request;
    request.url = current_url_;
    request.method = HttpMethod::GET;

    LOG_INFO("NetworkDemuxer", "Starting download thread");

    // TODO: Implement actual HTTP download to ring buffer
    // This is a simplified placeholder

    while (!should_stop_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int NetworkDemuxer::ReadCallback(void* opaque, uint8_t* buf, int buf_size) {
    auto* demuxer = static_cast<NetworkDemuxer*>(opaque);
    return static_cast<int>(demuxer->ring_buffer_->Read(buf, buf_size));
}

int64_t NetworkDemuxer::SeekCallback(void* opaque, int64_t offset, int whence) {
    auto* demuxer = static_cast<NetworkDemuxer*>(opaque);

    if (whence == AVSEEK_SIZE) {
        return -1; // Unknown size for streaming
    }

    // Network seeking not supported in basic implementation
    return -1;
}

std::unique_ptr<IDemuxer> CreateNetworkDemuxer() {
    return std::make_unique<NetworkDemuxer>();
}

} // namespace avsdk
