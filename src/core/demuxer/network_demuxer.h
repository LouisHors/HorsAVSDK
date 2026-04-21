#pragma once
#include "avsdk/demuxer.h"
#include "avsdk/network/http_client.h"
#include "avsdk/ring_buffer.h"
#include <thread>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
}

namespace avsdk {

class NetworkDemuxer : public IDemuxer {
public:
    NetworkDemuxer();
    ~NetworkDemuxer() override;

    ErrorCode Open(const std::string& url) override;
    void Close() override;
    AVPacketPtr ReadPacket() override;
    ErrorCode Seek(Timestamp position_ms) override;
    MediaInfo GetMediaInfo() const override;
    int GetVideoStreamIndex() const override { return video_stream_index_; }
    int GetAudioStreamIndex() const override { return audio_stream_index_; }
    AVCodecParameters* GetVideoCodecParameters() const override;
    AVCodecParameters* GetAudioCodecParameters() const override;

    double GetVideoTimebase() const override;
    double GetAudioTimebase() const override;

    // Network specific
    void SetBufferSize(size_t size);
    int64_t GetBufferedDuration() const;
    bool IsBuffering() const;

private:
    void DownloadThread();
    bool WaitForData(size_t min_size, int timeout_ms);
    static int ReadCallback(void* opaque, uint8_t* buf, int buf_size);
    static int64_t SeekCallback(void* opaque, int64_t offset, int whence);

    AVFormatContext* format_ctx_ = nullptr;
    AVIOContext* io_ctx_ = nullptr;
    uint8_t* avio_buffer_ = nullptr;

    std::unique_ptr<RingBuffer> ring_buffer_;
    std::unique_ptr<HttpClient> http_client_;
    std::thread download_thread_;
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_buffering_{false};
    std::atomic<ErrorCode> download_error_{ErrorCode::OK};

    std::string current_url_;
    MediaInfo media_info_;
    int64_t content_length_ = -1;
    int64_t total_downloaded_ = 0;
    int64_t current_range_start_ = -1;
    int video_stream_index_ = -1;
    int audio_stream_index_ = -1;
};

std::unique_ptr<IDemuxer> CreateNetworkDemuxer();

} // namespace avsdk
