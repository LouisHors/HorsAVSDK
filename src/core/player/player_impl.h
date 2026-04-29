#pragma once
#include "avsdk/player.h"
#include "avsdk/demuxer.h"
#include "avsdk/decoder.h"
#include "avsdk/renderer.h"
#include "avsdk/audio_renderer.h"
#include "avsdk/data_bypass.h"
#include "audio_clock.h"
#include <atomic>
#include <thread>
#include <vector>
#include <fstream>
#include <condition_variable>

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
}

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

    std::vector<AudioTrackInfo> GetAudioTracks() const override;
    ErrorCode SelectAudioTrack(int trackIndex) override;
    void SetMixAllAudioTracks(bool enable) override;
    bool GetMixAllAudioTracks() const override;

    ErrorCode SetRenderer(std::shared_ptr<IRenderer> renderer) override;
    void SetRenderView(void* native_window) override;

    // Callback registration
    void SetCallback(IPlayerCallback* callback) override;

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
    void VideoRenderLoop();
    void DispatchDecodedVideoFrame(const VideoFrame& frame);
    void DispatchDecodedAudioFrame(const AudioFrame& frame);
    void RenderVideoFrame(const AVFrame* frame);

    void NotifyStateChanged(PlayerState oldState, PlayerState newState);
    void NotifyError(ErrorCode error, const std::string& message);
    void NotifyPrepared();
    void NotifyComplete();
    void NotifyProgress(Timestamp position, Timestamp duration);
    void NotifySeekComplete(Timestamp position);
    void NotifyBuffering(bool isBuffering, int percent);

    std::unique_ptr<IDemuxer> demuxer_;
    std::unique_ptr<IDecoder> video_decoder_;
    std::vector<std::unique_ptr<IDecoder>> audio_decoders_;
    std::shared_ptr<IRenderer> video_renderer_;
    std::unique_ptr<IAudioRenderer> audio_renderer_;

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

    // Audio resampling
    SwrContext* swr_context_ = nullptr;
    std::vector<uint8_t> audio_resample_buffer_;

    // Debug: audio dump
    std::ofstream audio_dump_file_;
    bool audio_dump_initialized_ = false;
    int audio_resampled_samples_ = 0;

    // Audio clock for AV sync
    AudioClock audio_clock_;

    // Callback
    IPlayerCallback* callback_ = nullptr;
    std::mutex callback_mutex_;

    // Video frame queue for sync
    struct VideoFrameItem {
        AVFramePtr frame;
        double pts;  // in seconds
    };
    std::vector<VideoFrameItem> video_frame_queue_;
    std::mutex video_queue_mutex_;
    std::condition_variable video_queue_cv_;
    static constexpr size_t kMaxVideoQueueSize = 10;

    // Video render thread for AV sync
    std::thread video_render_thread_;
    std::atomic<bool> video_render_stop_{false};
    std::atomic<bool> playback_finished_{false};

    // Timebase for video stream
    double video_timebase_ = 0.0;
    std::vector<double> audio_timebases_;
    int selected_audio_track_ = 0;
    double first_audio_pts_ = -1.0;  // First audio PTS for sync baseline

    // Audio mixing
    bool mix_all_audio_tracks_ = false;
    // Each track's decoded PCM buffer (S16 interleaved samples)
    std::vector<std::vector<int16_t>> audio_track_mix_buffers_;
    void MixAndWriteAudioBuffers();

    // Hardware decoder fallback tracking
    bool hw_decoder_active_ = false;
};

} // namespace avsdk
