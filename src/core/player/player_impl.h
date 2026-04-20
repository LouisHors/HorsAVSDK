#pragma once
#include "avsdk/player.h"
#include "avsdk/demuxer.h"
#include "avsdk/decoder.h"
#include <atomic>
#include <thread>

namespace avsdk {

class PlayerImpl : public IPlayer {
public:
    PlayerImpl();
    ~PlayerImpl() override;

    ErrorCode Initialize(const PlayerConfig& config) override;
    ErrorCode Open(const std::string& url) override;
    void Close() override;

    ErrorCode Play() override;
    ErrorCode Pause() override;
    ErrorCode Stop() override;
    ErrorCode Seek(Timestamp position_ms) override;

    PlayerState GetState() const override;
    MediaInfo GetMediaInfo() const override;
    Timestamp GetCurrentPosition() const override;
    Timestamp GetDuration() const override;

private:
    void PlaybackLoop();

    std::unique_ptr<IDemuxer> demuxer_;
    std::unique_ptr<IDecoder> video_decoder_;
    std::unique_ptr<IDecoder> audio_decoder_;

    std::atomic<PlayerState> state_{PlayerState::kIdle};
    std::thread playback_thread_;
    std::atomic<bool> should_stop_{false};

    MediaInfo media_info_;
    PlayerConfig config_;
};

} // namespace avsdk
