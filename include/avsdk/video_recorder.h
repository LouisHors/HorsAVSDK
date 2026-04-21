#pragma once
#include <cstdint>
#include <string>
#include <memory>

namespace avsdk {

// Forward declaration
struct VideoFrame;
struct AudioFrame;
class IVideoRecorder;
class VideoRecorder;

// Recorder configuration
struct RecorderConfig {
    // Video settings
    int videoWidth = 1920;
    int videoHeight = 1080;
    int64_t videoBitrate = 5000000;  // 5 Mbps
    int frameRate = 30;
    std::string videoCodec = "libx264";

    // Audio settings
    int audioSampleRate = 48000;
    int audioChannels = 2;
    int64_t audioBitrate = 128000;   // 128 kbps
    std::string audioCodec = "aac";
};

// Interface for video recorder
class IVideoRecorder {
public:
    virtual ~IVideoRecorder() = default;

    // Start recording to file
    // path: output file path (e.g., "output.mp4", "output.mov", "output.mkv")
    // Returns 0 on success, negative error code on failure
    virtual int Start(const std::string& path) = 0;

    // Stop recording and close file
    virtual void Stop() = 0;

    // Check if currently recording
    virtual bool IsRecording() const = 0;

    // Get recording duration in milliseconds
    virtual int64_t GetRecordingDurationMs() const = 0;

    // Get recording statistics
    // videoFrames: number of video frames written
    // audioFrames: number of audio frames written
    virtual void GetStats(int64_t& videoFrames, int64_t& audioFrames) const = 0;
};

// Factory function to create video recorder
std::shared_ptr<IVideoRecorder> CreateVideoRecorder(const RecorderConfig& config);

} // namespace avsdk
