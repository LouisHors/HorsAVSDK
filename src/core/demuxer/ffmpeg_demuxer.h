#pragma once
#include "avsdk/demuxer.h"
extern "C" {
#include <libavformat/avformat.h>
}

namespace avsdk {

class FFmpegDemuxer : public IDemuxer {
public:
    FFmpegDemuxer();
    ~FFmpegDemuxer() override;

    ErrorCode Open(const std::string& url) override;
    void Close() override;
    AVPacketPtr ReadPacket() override;
    ErrorCode Seek(Timestamp position_ms) override;
    MediaInfo GetMediaInfo() const override;

private:
    AVFormatContext* format_ctx_ = nullptr;
    int video_stream_index_ = -1;
    int audio_stream_index_ = -1;
    MediaInfo media_info_;
};

} // namespace avsdk
