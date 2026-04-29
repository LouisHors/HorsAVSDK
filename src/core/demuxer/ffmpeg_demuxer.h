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
    int GetVideoStreamIndex() const override { return video_stream_index_; }
    int GetAudioStreamIndex() const override {
        return audio_stream_indices_.empty() ? -1 : audio_stream_indices_[0];
    }
    std::vector<int> GetAudioStreamIndices() const override { return audio_stream_indices_; }
    AVCodecParameters* GetVideoCodecParameters() const override;
    AVCodecParameters* GetAudioCodecParameters(int streamIndex = -1) const override;

    double GetVideoTimebase() const override;
    double GetAudioTimebase(int streamIndex = -1) const override;

private:
    AVFormatContext* format_ctx_ = nullptr;
    int video_stream_index_ = -1;
    std::vector<int> audio_stream_indices_;
    MediaInfo media_info_;
};

} // namespace avsdk
