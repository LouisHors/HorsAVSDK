#pragma once
#include <memory>
#include "avsdk/error.h"

namespace avsdk {

struct AudioFormat {
    int sample_rate = 48000;
    int channels = 2;
    int bits_per_sample = 16;
};

enum class RendererState {
    kStopped,
    kPlaying,
    kPaused
};

class IAudioRenderer {
public:
    virtual ~IAudioRenderer() = default;

    virtual ErrorCode Initialize(const AudioFormat& format) = 0;
    virtual void Close() = 0;

    virtual ErrorCode Play() = 0;
    virtual ErrorCode Pause() = 0;
    virtual ErrorCode Stop() = 0;

    virtual RendererState GetState() const = 0;
    virtual int GetVolume() const = 0;
    virtual void SetVolume(int volume) = 0;

    virtual int WriteAudio(const uint8_t* data, size_t size) = 0;
    virtual int GetBufferedDuration() const = 0;
};

std::unique_ptr<IAudioRenderer> CreateAudioUnitRenderer();

} // namespace avsdk
