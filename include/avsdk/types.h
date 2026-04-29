#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace avsdk {

using Timestamp = int64_t;

struct VideoResolution {
    int width;
    int height;
};

struct AudioTrackInfo {
    int stream_index = -1;
    std::string language;
    std::string title;
    int sample_rate = 0;
    int channels = 0;
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
    std::vector<AudioTrackInfo> audio_tracks;
};

// Data bypass related types
struct EncodedPacket {
    uint8_t* data;
    size_t size;
    int64_t pts;
    int64_t dts;
    bool keyFrame;
    int streamIndex;
};

struct VideoFrame {
    uint8_t* data[4];
    int linesize[4];
    VideoResolution resolution;
    int format;
    int64_t pts;
};

struct AudioFrame {
    uint8_t* data;
    int size;           // Data size in bytes
    int samples;
    int format;
    int sampleRate;
    int channels;
    int64_t pts;
};

} // namespace avsdk
