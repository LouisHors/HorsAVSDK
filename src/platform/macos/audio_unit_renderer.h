#pragma once
#include "avsdk/audio_renderer.h"
#import <AudioUnit/AudioUnit.h>
#include <mutex>
#include <vector>

namespace avsdk {

class AudioUnitRenderer : public IAudioRenderer {
public:
    AudioUnitRenderer();
    ~AudioUnitRenderer() override;

    ErrorCode Initialize(const AudioFormat& format) override;
    void Close() override;

    ErrorCode Play() override;
    ErrorCode Pause() override;
    ErrorCode Stop() override;

    RendererState GetState() const override { return state_; }
    int GetVolume() const override { return volume_; }
    void SetVolume(int volume) override;

    int WriteAudio(const uint8_t* data, size_t size) override;
    int GetBufferedDuration() const override;

private:
    static OSStatus RenderCallback(void* inRefCon,
                                   AudioUnitRenderActionFlags* ioActionFlags,
                                   const AudioTimeStamp* inTimeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList* ioData);

    AudioComponentInstance audio_unit_ = nullptr;
    AudioFormat format_;
    RendererState state_ = RendererState::kStopped;
    int volume_ = 100;

    std::vector<uint8_t> audio_buffer_;
    size_t read_offset_ = 0;
    mutable std::mutex buffer_mutex_;
};

} // namespace avsdk
