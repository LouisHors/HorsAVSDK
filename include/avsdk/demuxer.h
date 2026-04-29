#pragma once
#include <string>
#include <memory>
#include "avsdk/types.h"
#include "avsdk/error.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace avsdk {

using AVPacketPtr = std::shared_ptr<AVPacket>;

class IDemuxer {
public:
    virtual ~IDemuxer() = default;

    virtual ErrorCode Open(const std::string& url) = 0;
    virtual void Close() = 0;
    virtual AVPacketPtr ReadPacket() = 0;
    virtual ErrorCode Seek(Timestamp position_ms) = 0;
    virtual MediaInfo GetMediaInfo() const = 0;
    virtual int GetVideoStreamIndex() const = 0;
    virtual int GetAudioStreamIndex() const = 0;
    virtual std::vector<int> GetAudioStreamIndices() const = 0;
    virtual AVCodecParameters* GetVideoCodecParameters() const = 0;
    virtual AVCodecParameters* GetAudioCodecParameters(int streamIndex = -1) const = 0;

    // Get stream timebase for PTS calculation
    virtual double GetVideoTimebase() const = 0;
    virtual double GetAudioTimebase(int streamIndex = -1) const = 0;
};

std::unique_ptr<IDemuxer> CreateFFmpegDemuxer();
std::unique_ptr<IDemuxer> CreateNetworkDemuxer();

} // namespace avsdk
