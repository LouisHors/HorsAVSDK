#include "audio_clock.h"
#include "avsdk/logger.h"

namespace avsdk {

void AudioClock::Start() {
    if (!is_running_.exchange(true)) {
        start_time_ = std::chrono::steady_clock::now();
    }
}

void AudioClock::Stop() {
    is_running_ = false;
}

void AudioClock::Reset() {
    is_running_ = false;
    base_pts_ = 0.0;
    total_samples_played_ = 0;
    sample_rate_ = 0;
}

void AudioClock::UpdateBySamples(int samplesPlayed, int sampleRate) {
    if (!is_running_.load()) return;
    total_samples_played_ += samplesPlayed;
    sample_rate_ = sampleRate;
}

void AudioClock::UpdateByPTS(double pts) {
    base_pts_ = pts;
    if (is_running_.load()) {
        start_time_ = std::chrono::steady_clock::now();
    }
}

double AudioClock::GetTime() const {
    if (!is_running_.load()) {
        return base_pts_.load();
    }
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - start_time_).count();
    return base_pts_.load() + elapsed;
}

int64_t AudioClock::GetTimeMs() const {
    return static_cast<int64_t>(GetTime() * 1000.0);
}

int64_t AudioClock::GetVideoDelayMs(double videoPTS) const {
    double currentTime = GetTime();
    double delay = videoPTS - currentTime;
    return static_cast<int64_t>(delay * 1000.0);
}

} // namespace avsdk
