#pragma once
#include <string>
#include <memory>
#include "avsdk/types.h"
#include "avsdk/error.h"

struct AVPacket;

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
};

std::unique_ptr<IDemuxer> CreateFFmpegDemuxer();

} // namespace avsdk
