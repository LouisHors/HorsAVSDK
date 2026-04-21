#pragma once
#include "avsdk/video_recorder.h"
#include "avsdk/data_bypass.h"
#include "avsdk/types.h"
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>

extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct AVPacket;
struct SwsContext;
struct SwrContext;
}

namespace avsdk {

// VideoRecorder implementation
class VideoRecorder : public IVideoRecorder, public IDataBypass {
public:
    explicit VideoRecorder(const RecorderConfig& config);
    ~VideoRecorder() override;

    // IVideoRecorder interface
    int Start(const std::string& path) override;
    void Stop() override;
    bool IsRecording() const override;
    int64_t GetRecordingDurationMs() const override;
    void GetStats(int64_t& videoFrames, int64_t& audioFrames) const override;

    // IDataBypass interface - receives decoded frames
    void OnRawPacket(const EncodedPacket& packet) override;
    void OnDecodedVideoFrame(const VideoFrame& frame) override;
    void OnDecodedAudioFrame(const AudioFrame& frame) override;
    void OnPreRenderVideoFrame(VideoFrame& frame) override;
    void OnPreRenderAudioFrame(AudioFrame& frame) override;
    void OnEncodedPacket(const EncodedPacket& packet) override;

    // Static Create method
    static std::shared_ptr<VideoRecorder> Create(const RecorderConfig& config);

private:
    RecorderConfig config_;

    // FFmpeg context pointers (opaque to keep header clean)
    AVFormatContext* formatCtx_;
    AVCodecContext* videoCodecCtx_;
    AVCodecContext* audioCodecCtx_;
    AVStream* videoStream_;
    AVStream* audioStream_;

    // Format conversion contexts
    SwsContext* swsCtx_;        // For video format conversion
    SwrContext* swrCtx_;        // For audio resampling

    // State
    mutable std::mutex mutex_;
    bool isRecording_;
    int64_t startTimeMs_;
    int64_t videoFramesWritten_;
    int64_t audioFramesWritten_;
    int64_t videoPts_;
    int64_t audioPts_;

    // Initialize video encoder
    int InitializeVideo();

    // Initialize audio encoder
    int InitializeAudio();

    // Write video frame
    int WriteVideoFrame(const VideoFrame& frame);

    // Write audio frame
    int WriteAudioFrame(const AudioFrame& frame);

    // Cleanup all resources
    void Cleanup();

    // Detect output format from file extension
    std::string GetOutputFormatFromPath(const std::string& path) const;
};

} // namespace avsdk
