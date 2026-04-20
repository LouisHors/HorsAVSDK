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

    // Network specific
    void SetBufferSize(size_t size);
    int64_t GetBufferedDuration() const;
    bool IsBuffering() const;

private:
    void DownloadThread();
    static int ReadCallback(void* opaque, uint8_t* buf, int buf_size);
    static int64_t SeekCallback(void* opaque, int64_t offset, int whence);

    AVFormatContext* format_ctx_ = nullptr;
    AVIOContext* io_ctx_ = nullptr;
    uint8_t* avio_buffer_ = nullptr;

    std::unique_ptr<RingBuffer> ring_buffer_;
    std::thread download_thread_;
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_buffering_{false};

    HttpClient http_client_;
    std::string current_url_;
    MediaInfo media_info_;
    int64_t buffered_duration_ = 0;
};

std::unique_ptr<IDemuxer> CreateNetworkDemuxer();

} // namespace avsdk
