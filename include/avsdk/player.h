#pragma once
#include <memory>
#include <string>
#include "avsdk/types.h"
#include "avsdk/error.h"
#include "avsdk/player_config.h"

// Forward declaration for DataBypass
namespace avsdk {
class DataBypassManager;
class IDataBypass;
}

namespace avsdk {

class IPlayer {
public:
    virtual ~IPlayer() = default;

    virtual ErrorCode Initialize(const PlayerConfig& config) = 0;
    virtual ErrorCode Open(const std::string& url) = 0;
    virtual void Close() = 0;

    virtual ErrorCode Play() = 0;
    virtual ErrorCode Pause() = 0;
    virtual ErrorCode Stop() = 0;
    virtual ErrorCode Seek(Timestamp position_ms) = 0;

    virtual PlayerState GetState() const = 0;
    virtual MediaInfo GetMediaInfo() const = 0;
    virtual Timestamp GetCurrentPosition() const = 0;
    virtual Timestamp GetDuration() const = 0;

    // Data Bypass methods (Phase 4)
    virtual void SetDataBypassManager(std::shared_ptr<DataBypassManager> manager) = 0;
    virtual std::shared_ptr<DataBypassManager> GetDataBypassManager() = 0;
    virtual ErrorCode SetDataBypass(std::shared_ptr<IDataBypass> bypass) = 0;
    virtual void EnableVideoFrameCallback(bool enable) = 0;
    virtual void EnableAudioFrameCallback(bool enable) = 0;
    virtual void SetCallbackVideoFormat(int format) = 0;  // AVPixelFormat
    virtual void SetCallbackAudioFormat(int format) = 0;  // AVSampleFormat
};

std::unique_ptr<IPlayer> CreatePlayer();

} // namespace avsdk
