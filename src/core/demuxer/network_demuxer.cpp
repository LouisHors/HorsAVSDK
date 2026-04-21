#include "network_demuxer.h"
#include "avsdk/logger.h"
#include <cstring>
#include <thread>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
}

namespace avsdk {

static constexpr size_t kAvioBufferSize = 32768;
static constexpr size_t kRingBufferSize = 8 * 1024 * 1024; // 8MB
static constexpr int kProbeDataSize = 64 * 1024; // 64KB for probing
static constexpr int kMaxWaitMs = 30000; // 30 seconds timeout

NetworkDemuxer::NetworkDemuxer()
    : ring_buffer_(std::make_unique<RingBuffer>(kRingBufferSize)),
      http_client_(std::make_unique<HttpClient>()) {}

NetworkDemuxer::~NetworkDemuxer() {
    Close();
}

ErrorCode NetworkDemuxer::Open(const std::string& url) {
    current_url_ = url;
    should_stop_ = false;
    is_buffering_ = true;
    total_downloaded_ = 0;
    content_length_ = -1;

    LOG_INFO("NetworkDemuxer", "Opening: " + url);

    // Start download thread
    download_thread_ = std::thread(&NetworkDemuxer::DownloadThread, this);

    // Wait for enough data to probe format
    if (!WaitForData(kProbeDataSize, kMaxWaitMs)) {
        LOG_ERROR("NetworkDemuxer", "Timeout waiting for initial data");
        Close();
        return ErrorCode::NetworkConnectFailed;
    }

    // Allocate AVIO buffer
    avio_buffer_ = static_cast<uint8_t*>(av_malloc(kAvioBufferSize));
    if (!avio_buffer_) {
        Close();
        return ErrorCode::OutOfMemory;
    }

    // Create custom IO context
    io_ctx_ = avio_alloc_context(avio_buffer_, kAvioBufferSize, 0, this,
                                  &NetworkDemuxer::ReadCallback,
                                  nullptr,
                                  &NetworkDemuxer::SeekCallback);
    if (!io_ctx_) {
        Close();
        return ErrorCode::OutOfMemory;
    }

    // Allocate format context
    format_ctx_ = avformat_alloc_context();
    if (!format_ctx_) {
        Close();
        return ErrorCode::OutOfMemory;
    }

    format_ctx_->pb = io_ctx_;

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

    // Find video and audio streams
    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        AVStream* stream = format_ctx_->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            media_info_.has_video = true;
            media_info_.video_width = stream->codecpar->width;
            media_info_.video_height = stream->codecpar->height;
            media_info_.video_framerate = av_q2d(stream->avg_frame_rate);
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index_ = i;
            media_info_.has_audio = true;
            media_info_.audio_sample_rate = stream->codecpar->sample_rate;
            media_info_.audio_channels = stream->codecpar->ch_layout.nb_channels;
        }
    }

    media_info_.url = url;
    media_info_.duration_ms = format_ctx_->duration * 1000 / AV_TIME_BASE;

    is_buffering_ = false;
    LOG_INFO("NetworkDemuxer", "Opened: " + url +
             " duration: " + std::to_string(media_info_.duration_ms) + "ms" +
             " video: " + (media_info_.has_video ? "yes" : "no") +
             " audio: " + (media_info_.has_audio ? "yes" : "no"));

    return ErrorCode::OK;
}

void NetworkDemuxer::Close() {
    should_stop_ = true;

    if (http_client_) {
        http_client_->Cancel();
    }

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
        avio_buffer_ = nullptr;
    }

    if (avio_buffer_) {
        av_free(avio_buffer_);
        avio_buffer_ = nullptr;
    }

    // Reset state
    is_buffering_ = false;
    total_downloaded_ = 0;
    content_length_ = -1;
    video_stream_index_ = -1;
    audio_stream_index_ = -1;
}

bool NetworkDemuxer::WaitForData(size_t min_size, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();

    while (ring_buffer_->AvailableToRead() < min_size) {
        if (should_stop_) {
            return false;
        }

        // Check if download failed (connection error)
        if (download_error_ != ErrorCode::OK) {
            return false;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        if (elapsed > timeout_ms) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
}

AVPacketPtr NetworkDemuxer::ReadPacket() {
    if (!format_ctx_) return nullptr;

    // Wait for data if buffer is running low
    if (is_buffering_) {
        // Don't block indefinitely, just check if we should continue
        if (ring_buffer_->AvailableToRead() < kProbeDataSize && !should_stop_) {
            // Still buffering, but allow some packets through
        }
    }

    AVPacket* packet = av_packet_alloc();
    int ret = av_read_frame(format_ctx_, packet);

    if (ret < 0) {
        av_packet_free(&packet);

        // Check if we need more data
        if (ret == AVERROR(EAGAIN)) {
            // Wait for more data
            if (WaitForData(kProbeDataSize / 4, 1000)) {
                // Try again
                return ReadPacket();
            }
        }

        return nullptr;
    }

    return AVPacketPtr(packet, [](AVPacket* p) { av_packet_free(&p); });
}

ErrorCode NetworkDemuxer::Seek(Timestamp position_ms) {
    if (!format_ctx_) return ErrorCode::NotInitialized;

    // For network streams, seeking may require HTTP range request
    if (content_length_ > 0) {
        // Calculate byte offset (approximate)
        int64_t duration_ms = media_info_.duration_ms;
        if (duration_ms > 0) {
            double position_ratio = static_cast<double>(position_ms) / duration_ms;
            int64_t byte_offset = static_cast<int64_t>(position_ratio * content_length_);

            // Restart download with range request
            should_stop_ = true;
            if (download_thread_.joinable()) {
                download_thread_.join();
            }

            ring_buffer_->Clear();
            total_downloaded_ = 0;
            should_stop_ = false;
            current_range_start_ = byte_offset;

            download_thread_ = std::thread(&NetworkDemuxer::DownloadThread, this);

            // Wait for data
            if (!WaitForData(kProbeDataSize, 5000)) {
                return ErrorCode::NetworkConnectFailed;
            }
        }
    }

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

AVCodecParameters* NetworkDemuxer::GetAudioCodecParameters() const {
    if (!format_ctx_ || audio_stream_index_ < 0) {
        return nullptr;
    }
    return format_ctx_->streams[audio_stream_index_]->codecpar;
}

double NetworkDemuxer::GetVideoTimebase() const {
    if (!format_ctx_ || video_stream_index_ < 0) {
        return 0.0;
    }
    AVStream* stream = format_ctx_->streams[video_stream_index_];
    return av_q2d(stream->time_base);
}

double NetworkDemuxer::GetAudioTimebase() const {
    if (!format_ctx_ || audio_stream_index_ < 0) {
        return 0.0;
    }
    AVStream* stream = format_ctx_->streams[audio_stream_index_];
    return av_q2d(stream->time_base);
}

void NetworkDemuxer::SetBufferSize(size_t size) {
    // TODO: Implement dynamic buffer resize
}

int64_t NetworkDemuxer::GetBufferedDuration() const {
    // Estimate based on buffer size and bitrate
    if (!format_ctx_ || media_info_.duration_ms <= 0) {
        return 0;
    }

    size_t buffer_size = ring_buffer_->AvailableToRead();
    double buffer_ratio = static_cast<double>(buffer_size) / kRingBufferSize;
    return static_cast<int64_t>(buffer_ratio * media_info_.duration_ms);
}

bool NetworkDemuxer::IsBuffering() const {
    return is_buffering_.load();
}

void NetworkDemuxer::DownloadThread() {
    HttpRequest request;
    request.url = current_url_;
    request.method = HttpMethod::GET;

    if (current_range_start_ > 0) {
        request.range_start = current_range_start_;
    }

    LOG_INFO("NetworkDemuxer", "Starting download from: " + current_url_ +
             (request.range_start >= 0 ? " (range: " + std::to_string(request.range_start) + ")" : ""));

    try {
        http_client_->DownloadAsync(
            request,
            [this](const uint8_t* data, size_t size) {
                // Data callback
                size_t written = 0;
                while (written < size && !should_stop_) {
                    size_t to_write = size - written;
                    size_t available = ring_buffer_->AvailableToWrite();

                    if (available == 0) {
                        // Buffer full, wait a bit
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }

                    if (to_write > available) {
                        to_write = available;
                    }

                    size_t actual = ring_buffer_->Write(data + written, to_write);
                    written += actual;
                    total_downloaded_ += actual;
                }
            },
            [this](int64_t downloaded, int64_t total) {
                // Progress callback
                content_length_ = total;
                if (total > 0) {
                    int progress = static_cast<int>((downloaded * 100) / total);
                    if (progress % 10 == 0) {
                        LOG_INFO("NetworkDemuxer", "Download progress: " + std::to_string(progress) + "%");
                    }
                }
            }
        );
    } catch (const std::exception& e) {
        LOG_ERROR("NetworkDemuxer", "Download exception: " + std::string(e.what()));
        download_error_ = ErrorCode::NetworkConnectFailed;
    }

    LOG_INFO("NetworkDemuxer", "Download thread ended, total downloaded: " + std::to_string(total_downloaded_) + " bytes");
}

int NetworkDemuxer::ReadCallback(void* opaque, uint8_t* buf, int buf_size) {
    auto* demuxer = static_cast<NetworkDemuxer*>(opaque);

    // Check if download failed
    if (demuxer->download_error_ != ErrorCode::OK) {
        return AVERROR(EIO);
    }

    // Read from ring buffer
    size_t available = demuxer->ring_buffer_->AvailableToRead();
    if (available == 0) {
        // No data available - return EAGAIN to indicate should try again
        return AVERROR(EAGAIN);
    }

    size_t to_read = std::min(static_cast<size_t>(buf_size), available);
    size_t read = demuxer->ring_buffer_->Read(buf, to_read);

    return static_cast<int>(read);
}

int64_t NetworkDemuxer::SeekCallback(void* opaque, int64_t offset, int whence) {
    auto* demuxer = static_cast<NetworkDemuxer*>(opaque);

    if (whence == AVSEEK_SIZE) {
        return demuxer->content_length_;
    }

    // For live streams or when content length is unknown, seeking is not supported
    if (demuxer->content_length_ <= 0) {
        return -1;
    }

    // Simple seeking within the ring buffer is not really possible
    // Real implementation would need to restart download with range request
    // This is handled in the Seek() method
    return -1;
}

std::unique_ptr<IDemuxer> CreateNetworkDemuxer() {
    return std::make_unique<NetworkDemuxer>();
}

} // namespace avsdk
