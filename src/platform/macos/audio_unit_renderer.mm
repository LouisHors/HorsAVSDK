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

    // Reset buffer
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        audio_buffer_.clear();
        read_offset_ = 0;
    }

    LOG_INFO("AudioUnit", "Initialized " + std::to_string(format.sample_rate) + "Hz audio");
    return ErrorCode::OK;
}

void AudioUnitRenderer::Close() {
    Stop();

    if (audio_unit_) {
        AudioUnitUninitialize(audio_unit_);
        AudioComponentInstanceDispose(audio_unit_);
        audio_unit_ = nullptr;
    }
}

ErrorCode AudioUnitRenderer::Play() {
    if (!audio_unit_) return ErrorCode::NotInitialized;

    // Wait for enough data (at least 0.3 seconds)
    int wait_count = 0;
    while (wait_count < 100) {
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            size_t buffered = audio_buffer_.size() - read_offset_;
            size_t bytes_300ms = format_.sample_rate * format_.channels * (format_.bits_per_sample / 8) * 3 / 10;
            if (buffered >= bytes_300ms) break;
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

    std::lock_guard<std::mutex> lock(buffer_mutex_);
    audio_buffer_.clear();
    read_offset_ = 0;

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
    if (size == 0) return 0;

    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // Simple: append to buffer
    size_t old_size = audio_buffer_.size();
    audio_buffer_.resize(old_size + size);
    std::memcpy(audio_buffer_.data() + old_size, data, size);

    return static_cast<int>(size);
}

int AudioUnitRenderer::GetBufferedDuration() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    size_t remaining = audio_buffer_.size() - read_offset_;
    int bytes_per_ms = (format_.sample_rate * format_.channels * (format_.bits_per_sample / 8)) / 1000;
    if (bytes_per_ms == 0) return 0;
    return static_cast<int>(remaining / bytes_per_ms);
}

OSStatus AudioUnitRenderer::RenderCallback(void* inRefCon,
                                           AudioUnitRenderActionFlags* ioActionFlags,
                                           const AudioTimeStamp* inTimeStamp,
                                           UInt32 inBusNumber,
                                           UInt32 inNumberFrames,
                                           AudioBufferList* ioData) {
    auto* renderer = static_cast<AudioUnitRenderer*>(inRefCon);

    AudioBuffer* buffer = &ioData->mBuffers[0];
    uint8_t* outBuffer = static_cast<uint8_t*>(buffer->mData);

    size_t bytes_per_frame = renderer->format_.channels * (renderer->format_.bits_per_sample / 8);
    size_t bytes_needed = inNumberFrames * bytes_per_frame;

    // Quick lock - minimize time in callback
    size_t to_copy = 0;
    size_t read_pos = 0;
    {
        std::lock_guard<std::mutex> lock(renderer->buffer_mutex_);
        size_t available = renderer->audio_buffer_.size() - renderer->read_offset_;
        to_copy = std::min(bytes_needed, available);
        read_pos = renderer->read_offset_;
        renderer->read_offset_ += to_copy;
    }

    // Copy data outside of lock (data won't be modified by writer until we release)
    if (to_copy > 0) {
        std::memcpy(outBuffer, renderer->audio_buffer_.data() + read_pos, to_copy);
    }

    // Fill remaining with silence
    if (to_copy < bytes_needed) {
        std::memset(outBuffer + to_copy, 0, bytes_needed - to_copy);
    }

    // Periodically trim consumed buffer (not in every callback)
    static thread_local int call_count = 0;
    if (++call_count % 100 == 0) {
        std::lock_guard<std::mutex> lock(renderer->buffer_mutex_);
        if (renderer->read_offset_ > renderer->audio_buffer_.size() / 2) {
            // Move remaining data to front
            size_t remaining = renderer->audio_buffer_.size() - renderer->read_offset_;
            if (remaining > 0) {
                std::memmove(renderer->audio_buffer_.data(),
                            renderer->audio_buffer_.data() + renderer->read_offset_,
                            remaining);
            }
            renderer->audio_buffer_.resize(remaining);
            renderer->read_offset_ = 0;
        }
    }

    return noErr;
}

std::unique_ptr<IAudioRenderer> CreateAudioUnitRenderer() {
    return std::make_unique<AudioUnitRenderer>();
}

} // namespace avsdk
