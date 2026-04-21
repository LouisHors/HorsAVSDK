#include "audio_clock.h"
#include "avsdk/logger.h"

namespace avsdk {

void AudioClock::Start() {
    if (!is_running_.exchange(true)) {
        std::lock_guard<std::mutex> lock(mutex_);
        start_time_ = std::chrono::steady_clock::now();
    }
}

void AudioClock::Stop() {
    if (is_running_.exchange(false)) {
        std::lock_guard<std::mutex> lock(mutex_);
        // Accumulate elapsed time
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - start_time_).count();
        accumulated_time_ = accumulated_time_.load() + elapsed;
    }
}

void AudioClock::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_running_ = false;
    base_pts_ = 0.0;
    accumulated_time_ = 0.0;
    total_samples_played_ = 0;
}

void AudioClock::UpdateBySamples(int samplesPlayed, int sampleRate) {
    if (!is_running_.load()) return;

    total_samples_played_ += samplesPlayed;
    sample_rate_ = sampleRate;

    // Calculate expected time based on samples
    double expectedTime = static_cast<double>(total_samples_played_.load()) / sampleRate;

    // Update accumulated time to match sample-based time (drift correction)
    std::lock_guard<std::mutex> lock(mutex_);
    accumulated_time_ = expectedTime;
    start_time_ = std::chrono::steady_clock::now();
}

void AudioClock::UpdateByPTS(double pts) {
    std::lock_guard<std::mutex> lock(mutex_);
    base_pts_ = pts;
    accumulated_time_ = 0.0;
    if (is_running_.load()) {
        start_time_ = std::chrono::steady_clock::now();
    }
}

double AudioClock::GetTime() const {
    if (!is_running_.load()) {
        return base_pts_.load() + accumulated_time_.load();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - start_time_).count();
    return base_pts_.load() + accumulated_time_.load() + elapsed;
}

int64_t AudioClock::GetTimeMs() const {
    return static_cast<int64_t>(GetTime() * 1000.0);
}

int64_t AudioClock::GetVideoDelayMs(double videoPTS) const {
    double currentTime = GetTime();
    double delay = videoPTS - currentTime;
    return static_cast<int64_t>(delay * 1000.0);  // Convert to milliseconds
}

} // namespace avsdk
