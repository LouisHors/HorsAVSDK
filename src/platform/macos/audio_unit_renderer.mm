#import "audio_unit_renderer.h"
#include "avsdk/logger.h"
#import <AudioUnit/AudioUnit.h>

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
    stream_format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    stream_format.mBytesPerPacket = format.channels * (format.bits_per_sample / 8);
    stream_format.mFramesPerPacket = 1;
    stream_format.mBytesPerFrame = stream_format.mBytesPerPacket;
    stream_format.mChannelsPerFrame = format.channels;
    stream_format.mBitsPerChannel = format.bits_per_sample;

    status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input, 0, &stream_format, sizeof(stream_format));
    if (status != noErr) {
        LOG_ERROR("AudioUnit", "Failed to set stream format");
        return ErrorCode::HardwareNotAvailable;
    }

    AURenderCallbackStruct callback_struct;
    callback_struct.inputProc = RenderCallback;
    callback_struct.inputProcRefCon = this;

    status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_SetRenderCallback,
                                   kAudioUnitScope_Input, 0, &callback_struct, sizeof(callback_struct));
    if (status != noErr) {
        LOG_ERROR("AudioUnit", "Failed to set render callback");
        return ErrorCode::HardwareNotAvailable;
    }

    status = AudioUnitInitialize(audio_unit_);
    if (status != noErr) {
        LOG_ERROR("AudioUnit", "Failed to initialize audio unit");
        return ErrorCode::HardwareNotAvailable;
    }

    // Allocate 1 second buffer
    size_t buffer_size = format.sample_rate * format.channels * (format.bits_per_sample / 8);
    audio_buffer_.resize(buffer_size);

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

    OSStatus status = AudioOutputUnitStart(audio_unit_);
    if (status != noErr) {
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
    write_pos_ = 0;
    read_pos_ = 0;
    buffered_size_ = 0;

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
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    size_t written = 0;
    while (written < size && buffered_size_ < audio_buffer_.size()) {
        size_t pos = write_pos_ % audio_buffer_.size();
        size_t chunk = std::min(size - written, audio_buffer_.size() - pos);
        std::copy(data + written, data + written + chunk, audio_buffer_.begin() + pos);
        write_pos_ += chunk;
        written += chunk;
        buffered_size_ += chunk;
    }

    return static_cast<int>(written);
}

int AudioUnitRenderer::GetBufferedDuration() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    int bytes_per_ms = (format_.sample_rate * format_.channels * (format_.bits_per_sample / 8)) / 1000;
    if (bytes_per_ms == 0) return 0;
    return static_cast<int>(buffered_size_ / bytes_per_ms);
}

OSStatus AudioUnitRenderer::RenderCallback(void* inRefCon,
                                           AudioUnitRenderActionFlags* ioActionFlags,
                                           const AudioTimeStamp* inTimeStamp,
                                           UInt32 inBusNumber,
                                           UInt32 inNumberFrames,
                                           AudioBufferList* ioData) {
    auto* renderer = static_cast<AudioUnitRenderer*>(inRefCon);
    std::lock_guard<std::mutex> lock(renderer->buffer_mutex_);

    AudioBuffer* buffer = &ioData->mBuffers[0];
    UInt32 bytes_to_copy = inNumberFrames * renderer->format_.channels * (renderer->format_.bits_per_sample / 8);

    bytes_to_copy = std::min(bytes_to_copy, static_cast<UInt32>(renderer->buffered_size_));
    bytes_to_copy = std::min(bytes_to_copy, buffer->mDataByteSize);

    if (bytes_to_copy > 0) {
        size_t pos = renderer->read_pos_ % renderer->audio_buffer_.size();
        size_t chunk = std::min(static_cast<size_t>(bytes_to_copy), renderer->audio_buffer_.size() - pos);

        std::copy(renderer->audio_buffer_.begin() + pos,
                  renderer->audio_buffer_.begin() + pos + chunk,
                  static_cast<uint8_t*>(buffer->mData));

        if (bytes_to_copy > chunk) {
            std::copy(renderer->audio_buffer_.begin(),
                      renderer->audio_buffer_.begin() + (bytes_to_copy - chunk),
                      static_cast<uint8_t*>(buffer->mData) + chunk);
        }

        renderer->read_pos_ += bytes_to_copy;
        renderer->buffered_size_ -= bytes_to_copy;
        buffer->mDataByteSize = bytes_to_copy;
    } else {
        memset(buffer->mData, 0, buffer->mDataByteSize);
    }

    return noErr;
}

std::unique_ptr<IAudioRenderer> CreateAudioUnitRenderer() {
    return std::make_unique<AudioUnitRenderer>();
}

} // namespace avsdk
