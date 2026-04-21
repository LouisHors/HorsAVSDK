#import "audio_unit_renderer.h"
#include "avsdk/logger.h"
#import <AudioUnit/AudioUnit.h>
#include <thread>
#include <chrono>
#include <cstring>

namespace avsdk {

AudioUnitRenderer::AudioUnitRenderer() = default;

AudioUnitRenderer::~AudioUnitRenderer() {
    Close();
}

ErrorCode AudioUnitRenderer::Initialize(const AudioFormat& format) {
    format_ = format;

    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) {
        LOG_ERROR("AudioUnit", "Failed to find audio component");
        return ErrorCode::HardwareNotAvailable;
    }

    OSStatus status = AudioComponentInstanceNew(comp, &audio_unit_);
    if (status != noErr) {
        LOG_ERROR("AudioUnit", "Failed to create audio unit");
        return ErrorCode::HardwareNotAvailable;
    }

    AudioStreamBasicDescription stream_format = {};
    stream_format.mSampleRate = format.sample_rate;
    stream_format.mFormatID = kAudioFormatLinearPCM;
    stream_format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
    stream_format.mBytesPerPacket = format.channels * (format.bits_per_sample / 8);
    stream_format.mFramesPerPacket = 1;
    stream_format.mBytesPerFrame = stream_format.mBytesPerPacket;
    stream_format.mChannelsPerFrame = format.channels;
    stream_format.mBitsPerChannel = format.bits_per_sample;

    status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input, 0, &stream_format, sizeof(stream_format));
    if (status != noErr) {
        LOG_ERROR("AudioUnit", "Failed to set stream format: " + std::to_string(status));
        return ErrorCode::HardwareNotAvailable;
    }

    AURenderCallbackStruct callback_struct;
    callback_struct.inputProc = RenderCallback;
    callback_struct.inputProcRefCon = this;

    status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_SetRenderCallback,
                                   kAudioUnitScope_Input, 0, &callback_struct, sizeof(callback_struct));
    if (status != noErr) {
        LOG_ERROR("AudioUnit", "Failed to set render callback: " + std::to_string(status));
        return ErrorCode::HardwareNotAvailable;
    }

    status = AudioUnitInitialize(audio_unit_);
    if (status != noErr) {
        LOG_ERROR("AudioUnit", "Failed to initialize audio unit: " + std::to_string(status));
        return ErrorCode::HardwareNotAvailable;
    }

    // Create ring buffer
    ring_buffer_ = std::make_unique<RingBuffer>(256 * 1024);

    LOG_INFO("AudioUnit", "Initialized " + std::to_string(format.sample_rate) + "Hz audio with ring buffer");
    return ErrorCode::OK;
}

void AudioUnitRenderer::Close() {
    Stop();

    if (audio_unit_) {
        AudioUnitUninitialize(audio_unit_);
        AudioComponentInstanceDispose(audio_unit_);
        audio_unit_ = nullptr;
    }

    ring_buffer_.reset();
}

ErrorCode AudioUnitRenderer::Play() {
    if (!audio_unit_) return ErrorCode::NotInitialized;

    // Wait for enough data (at least 0.3 seconds)
    int wait_count = 0;
    int bytes_per_300ms = format_.sample_rate * format_.channels * (format_.bits_per_sample / 8) * 3 / 10;
    while (wait_count < 100) {
        if (ring_buffer_->AvailableToRead() >= static_cast<size_t>(bytes_per_300ms)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        wait_count++;
    }

    OSStatus status = AudioOutputUnitStart(audio_unit_);
    if (status != noErr) {
        LOG_ERROR("AudioUnit", "Failed to start: " + std::to_string(status));
        return ErrorCode::HardwareNotAvailable;
    }

    state_ = RendererState::kPlaying;
    LOG_INFO("AudioUnit", "Started playback");
    return ErrorCode::OK;
}

ErrorCode AudioUnitRenderer::Pause() {
    if (!audio_unit_) return ErrorCode::NotInitialized;

    OSStatus status = AudioOutputUnitStop(audio_unit_);
    if (status != noErr) {
        return ErrorCode::HardwareNotAvailable;
    }

    state_ = RendererState::kPaused;
    LOG_INFO("AudioUnit", "Paused playback");
    return ErrorCode::OK;
}

ErrorCode AudioUnitRenderer::Stop() {
    if (!audio_unit_) return ErrorCode::OK;

    AudioOutputUnitStop(audio_unit_);

    if (ring_buffer_) {
        ring_buffer_->Clear();
    }

    state_ = RendererState::kStopped;
    LOG_INFO("AudioUnit", "Stopped playback");
    return ErrorCode::OK;
}

void AudioUnitRenderer::SetVolume(int volume) {
    volume_ = std::max(0, std::min(100, volume));

    if (audio_unit_) {
        float linear_volume = volume_ / 100.0f;
        AudioUnitSetParameter(audio_unit_, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, linear_volume, 0);
    }
}

int AudioUnitRenderer::WriteAudio(const uint8_t* data, size_t size) {
    if (!ring_buffer_ || size == 0) return 0;

    // Write to ring buffer (RingBuffer handles locking internally)
    size_t written = ring_buffer_->Write(data, size);
    return static_cast<int>(written);
}

int AudioUnitRenderer::GetBufferedDuration() const {
    if (!ring_buffer_) return 0;

    size_t available = ring_buffer_->AvailableToRead();
    int bytes_per_ms = (format_.sample_rate * format_.channels * (format_.bits_per_sample / 8)) / 1000;
    if (bytes_per_ms == 0) return 0;
    return static_cast<int>(available / bytes_per_ms);
}

OSStatus AudioUnitRenderer::RenderCallback(void* inRefCon,
                                           AudioUnitRenderActionFlags* ioActionFlags,
                                           const AudioTimeStamp* inTimeStamp,
                                           UInt32 inBusNumber,
                                           UInt32 inNumberFrames,
                                           AudioBufferList* ioData) {
    auto* renderer = static_cast<AudioUnitRenderer*>(inRefCon);

    if (!renderer->ring_buffer_) {
        // No ring buffer, fill silence
        AudioBuffer* buffer = &ioData->mBuffers[0];
        std::memset(buffer->mData, 0, buffer->mDataByteSize);
        return noErr;
    }

    AudioBuffer* buffer = &ioData->mBuffers[0];
    uint8_t* outBuffer = static_cast<uint8_t*>(buffer->mData);
    size_t bytes_needed = buffer->mDataByteSize;

    // Read directly from ring buffer (RingBuffer handles locking internally)
    size_t bytes_read = renderer->ring_buffer_->Read(outBuffer, bytes_needed);

    // Fill remaining with silence
    if (bytes_read < bytes_needed) {
        std::memset(outBuffer + bytes_read, 0, bytes_needed - bytes_read);
    }

    return noErr;
}

std::unique_ptr<IAudioRenderer> CreateAudioUnitRenderer() {
    return std::make_unique<AudioUnitRenderer>();
}

} // namespace avsdk
