#pragma once
#include "avsdk/player.h"
#include "avsdk/demuxer.h"
#include "avsdk/decoder.h"
#include "avsdk/renderer.h"
#include "avsdk/data_bypass.h"
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

    ErrorCode SetRenderer(std::shared_ptr<IRenderer> renderer) override;
    void SetRenderView(void* native_window) override;

    // Data Bypass methods (Phase 4)
    void SetDataBypassManager(std::shared_ptr<DataBypassManager> manager) override;
    std::shared_ptr<DataBypassManager> GetDataBypassManager() override;
    ErrorCode SetDataBypass(std::shared_ptr<IDataBypass> bypass) override;
    void EnableVideoFrameCallback(bool enable) override;
    void EnableAudioFrameCallback(bool enable) override;
    void SetCallbackVideoFormat(int format) override;
    void SetCallbackAudioFormat(int format) override;

private:
    void PlaybackLoop();
    void DispatchDecodedVideoFrame(const VideoFrame& frame);
    void DispatchDecodedAudioFrame(const AudioFrame& frame);
    void RenderVideoFrame(const AVFrame* frame);

    std::unique_ptr<IDemuxer> demuxer_;
    std::unique_ptr<IDecoder> video_decoder_;
    std::unique_ptr<IDecoder> audio_decoder_;
    std::shared_ptr<IRenderer> video_renderer_;

    std::atomic<PlayerState> state_{PlayerState::kIdle};
    std::thread playback_thread_;
    std::atomic<bool> should_stop_{false};

    MediaInfo media_info_;
    PlayerConfig config_;
    void* native_window_ = nullptr;

    // Data Bypass members
    std::shared_ptr<DataBypassManager> dataBypassManager_;
    bool enableVideoFrameCallback_ = false;
    bool enableAudioFrameCallback_ = false;
    int callbackVideoFormat_ = 0;  // AV_PIX_FMT_YUV420P
    int callbackAudioFormat_ = 0;  // AV_SAMPLE_FMT_S16
};

} // namespace avsdk
