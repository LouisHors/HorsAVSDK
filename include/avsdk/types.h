#pragma once
#include <cstdint>
#include <string>

namespace avsdk {

using Timestamp = int64_t;

struct VideoResolution {
    int width;
    int height;
};

struct MediaInfo {
    std::string url;
    std::string format;
    int64_t duration_ms;
    int video_width;
    int video_height;
    double video_framerate;
    int audio_sample_rate;
    int audio_channels;
    bool has_video;
    bool has_audio;
};

} // namespace avsdk
