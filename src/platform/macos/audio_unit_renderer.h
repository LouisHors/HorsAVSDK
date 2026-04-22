#pragma once
#include "avsdk/audio_renderer.h"
#include "avsdk/ring_buffer.h"
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

    std::unique_ptr<RingBuffer> ring_buffer_;
    static constexpr size_t kBufferCapacity = 2 * 1024 * 1024; // 2MB ring buffer for ~11 seconds of audio
};

} // namespace avsdk
