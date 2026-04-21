#pragma once
#include <atomic>
#include <chrono>
#include <mutex>

namespace avsdk {

// Audio clock for AV synchronization
// Audio is the master clock, video frames sync to audio
class AudioClock {
public:
    AudioClock() = default;

    // Start the clock
    void Start();

    // Stop/pause the clock
    void Stop();

    // Reset the clock
    void Reset();

    // Update clock based on audio samples played
    // Call this when audio data is written to the renderer
    void UpdateBySamples(int samplesPlayed, int sampleRate);

    // Update clock based on PTS (for initial sync)
    void UpdateByPTS(double pts);

    // Get current clock time in seconds
    double GetTime() const;

    // Get current clock time in milliseconds
    int64_t GetTimeMs() const;

    // Check if clock is running
    bool IsRunning() const { return is_running_.load(); }

    // Calculate how long to wait for a video frame with given PTS
    // Returns: wait time in milliseconds (negative means frame is late)
    int64_t GetVideoDelayMs(double videoPTS) const;

private:
    std::atomic<bool> is_running_{false};
    std::atomic<double> base_pts_{0.0};      // Base PTS when clock started
    std::atomic<double> accumulated_time_{0.0};  // Accumulated time since start
    std::chrono::steady_clock::time_point start_time_;
    mutable std::mutex mutex_;

    // Total samples played (for drift correction)
    std::atomic<int64_t> total_samples_played_{0};
    std::atomic<int> sample_rate_{0};
};

} // namespace avsdk
