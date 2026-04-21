#include "player_impl.h"
#include "avsdk/logger.h"

#include <chrono>
#include <thread>
#include <fstream>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

// Platform audio renderer factory
#if defined(__APPLE__)
#include "audio_unit_renderer.h"
#endif

namespace avsdk {

// Helper to create platform audio renderer
std::unique_ptr<IAudioRenderer> CreatePlatformAudioRenderer() {
#if defined(__APPLE__)
    return CreateAudioUnitRenderer();
#else
    return nullptr;
#endif
}

PlayerImpl::PlayerImpl() = default;

PlayerImpl::~PlayerImpl() {
    Stop();
    Close();
}

ErrorCode PlayerImpl::Initialize(const PlayerConfig& config) {
    config_ = config;
    state_ = PlayerState::kStopped;
    LOG_INFO("Player", "Initialized");
    return ErrorCode::OK;
}

void PlayerImpl::SetRenderView(void* native_window) {
    native_window_ = native_window;
    LOG_INFO("Player", "Render view set");
}

ErrorCode PlayerImpl::SetRenderer(std::shared_ptr<IRenderer> renderer) {
    video_renderer_ = renderer;
    LOG_INFO("Player", "Renderer set");
    return ErrorCode::OK;
}

ErrorCode PlayerImpl::Open(const std::string& url) {
    demuxer_ = CreateFFmpegDemuxer();
    auto result = demuxer_->Open(url);
    if (result != ErrorCode::OK) {
        return result;
    }

    media_info_ = demuxer_->GetMediaInfo();

    // Initialize decoders
    if (media_info_.has_video) {
        video_decoder_ = CreateFFmpegDecoder();
        AVCodecParameters* codecpar = demuxer_->GetVideoCodecParameters();
        auto result = video_decoder_->Initialize(codecpar);
        if (result != ErrorCode::OK) {
            LOG_ERROR("Player", "Failed to initialize video decoder");
            return result;
        }

        // Initialize renderer with native window and video dimensions
        if (video_renderer_ && native_window_) {
            result = video_renderer_->Initialize(native_window_,
                                                  media_info_.video_width,
                                                  media_info_.video_height);
            if (result != ErrorCode::OK) {
                LOG_ERROR("Player", "Failed to initialize video renderer");
                return result;
            }
        }
    }

    // Initialize audio decoder and renderer
    if (media_info_.has_audio) {
        audio_decoder_ = CreateFFmpegDecoder();
        AVCodecParameters* audioCodecpar = demuxer_->GetAudioCodecParameters();
        if (audioCodecpar) {
            auto result = audio_decoder_->Initialize(audioCodecpar);
            if (result != ErrorCode::OK) {
                LOG_ERROR("Player", "Failed to initialize audio decoder");
                audio_decoder_.reset();
            } else {
                // Initialize audio renderer
                audio_renderer_ = CreatePlatformAudioRenderer();
                if (audio_renderer_) {
                    AudioFormat audioFormat;
                    audioFormat.sample_rate = media_info_.audio_sample_rate;
                    audioFormat.channels = media_info_.audio_channels;
                    audioFormat.bits_per_sample = 16; // Default to 16-bit
                    result = audio_renderer_->Initialize(audioFormat);
                    if (result != ErrorCode::OK) {
                        LOG_ERROR("Player", "Failed to initialize audio renderer");
                        audio_renderer_.reset();
                    } else {
                        LOG_INFO("Player", "Audio initialized: " +
                                 std::to_string(audioFormat.sample_rate) + "Hz, " +
                                 std::to_string(audioFormat.channels) + " channels");

                        // Initialize audio resampler (convert any format to S16)
                        AVChannelLayout out_ch_layout, in_ch_layout;
                        av_channel_layout_default(&out_ch_layout, audioFormat.channels);
                        av_channel_layout_default(&in_ch_layout, audioFormat.channels);

                        int ret = swr_alloc_set_opts2(&swr_context_,
                            &out_ch_layout,                       // Output layout
                            AV_SAMPLE_FMT_S16,                    // Output format
                            audioFormat.sample_rate,              // Output rate
                            &in_ch_layout,                        // Input layout
                            AV_SAMPLE_FMT_FLTP,                   // Input format (AAC default)
                            audioFormat.sample_rate,              // Input rate
                            0, nullptr);

                        av_channel_layout_uninit(&out_ch_layout);
                        av_channel_layout_uninit(&in_ch_layout);

                        if (ret < 0) {
                            LOG_ERROR("Player", "Failed to create audio resampler");
                            swr_context_ = nullptr;
                        } else {
                            ret = swr_init(swr_context_);
                            if (ret < 0) {
                                LOG_ERROR("Player", "Failed to initialize audio resampler");
                                swr_free(&swr_context_);
                                swr_context_ = nullptr;
                            } else {
                                LOG_INFO("Player", "Audio resampler initialized");
                            }
                        }
                    }
                }
            }
        }
    }

    LOG_INFO("Player", "Opened: " + url);
    return ErrorCode::OK;
}

void PlayerImpl::Close() {
    Stop();

    // Close audio dump file
    if (audio_dump_file_.is_open()) {
        audio_dump_file_.close();
        LOG_INFO("Player", "Audio dump saved: /tmp/audio_dump.pcm");
    }
    audio_dump_initialized_ = false;

    if (demuxer_) {
        demuxer_->Close();
        demuxer_.reset();
    }
    video_decoder_.reset();
    audio_decoder_.reset();
    if (swr_context_) {
        swr_free(&swr_context_);
        swr_context_ = nullptr;
    }
    audio_resample_buffer_.clear();
    if (video_renderer_) {
        video_renderer_->Release();
        video_renderer_.reset();
    }
    if (audio_renderer_) {
        audio_renderer_->Close();
        audio_renderer_.reset();
    }
}

ErrorCode PlayerImpl::Play() {
    if (state_ == PlayerState::kPlaying) {
        return ErrorCode::OK;
    }

    state_ = PlayerState::kPlaying;
    should_stop_ = false;
    playback_thread_ = std::thread(&PlayerImpl::PlaybackLoop, this);

    LOG_INFO("Player", "Started playback");
    return ErrorCode::OK;
}

ErrorCode PlayerImpl::Pause() {
    state_ = PlayerState::kPaused;
    LOG_INFO("Player", "Paused");
    return ErrorCode::OK;
}

ErrorCode PlayerImpl::Stop() {
    state_ = PlayerState::kStopped;
    should_stop_ = true;

    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }

    LOG_INFO("Player", "Stopped");
    return ErrorCode::OK;
}

ErrorCode PlayerImpl::Seek(Timestamp position_ms) {
    if (!demuxer_) return ErrorCode::NotInitialized;
    return demuxer_->Seek(position_ms);
}

void PlayerImpl::SetDataBypassManager(std::shared_ptr<DataBypassManager> manager) {
    dataBypassManager_ = manager;
}

std::shared_ptr<DataBypassManager> PlayerImpl::GetDataBypassManager() {
    if (!dataBypassManager_) {
        dataBypassManager_ = std::make_shared<DataBypassManager>();
    }
    return dataBypassManager_;
}

ErrorCode PlayerImpl::SetDataBypass(std::shared_ptr<IDataBypass> bypass) {
    if (!bypass) {
        return ErrorCode::InvalidParameter;
    }
    GetDataBypassManager()->RegisterBypass(bypass);
    return ErrorCode::OK;
}

void PlayerImpl::EnableVideoFrameCallback(bool enable) {
    enableVideoFrameCallback_ = enable;
}

void PlayerImpl::EnableAudioFrameCallback(bool enable) {
    enableAudioFrameCallback_ = enable;
}

void PlayerImpl::SetCallbackVideoFormat(int format) {
    callbackVideoFormat_ = format;
}

void PlayerImpl::SetCallbackAudioFormat(int format) {
    callbackAudioFormat_ = format;
}

void PlayerImpl::DispatchDecodedVideoFrame(const VideoFrame& frame) {
    if (enableVideoFrameCallback_ && dataBypassManager_) {
        dataBypassManager_->DispatchDecodedVideoFrame(frame);
    }
}

void PlayerImpl::DispatchDecodedAudioFrame(const AudioFrame& frame) {
    if (enableAudioFrameCallback_ && dataBypassManager_) {
        dataBypassManager_->DispatchDecodedAudioFrame(frame);
    }
}

void PlayerImpl::RenderVideoFrame(const AVFrame* frame) {
    if (video_renderer_ && frame) {
        video_renderer_->RenderFrame(frame);
    }
}

void PlayerImpl::PlaybackLoop() {
    int videoStreamIndex = demuxer_->GetVideoStreamIndex();
    int audioStreamIndex = demuxer_->GetAudioStreamIndex();
    LOG_INFO("Player", "Playback loop started, video stream index: " + std::to_string(videoStreamIndex) +
             ", audio stream index: " + std::to_string(audioStreamIndex));

    // Pre-buffer audio before starting playback
    // This ensures audio playback doesn't start with empty buffer
    bool audio_prebuffered = false;
    int audio_packets_decoded = 0;

    while (!should_stop_ && state_ == PlayerState::kPlaying) {
        if (!demuxer_) break;

        auto packet = demuxer_->ReadPacket();
        if (!packet) {
            break; // End of stream
        }

        // Process video packets
        if (packet->stream_index == videoStreamIndex && videoStreamIndex >= 0) {
            // Decode video and render
            if (video_decoder_ && video_renderer_) {
                auto frame = video_decoder_->Decode(packet);
                if (frame) {
                    RenderVideoFrame(frame.get());

                    // Also dispatch for data bypass callbacks
                    VideoFrame vf;
                    vf.data[0] = frame->data[0];
                    vf.data[1] = frame->data[1];
                    vf.data[2] = frame->data[2];
                    vf.linesize[0] = frame->linesize[0];
                    vf.linesize[1] = frame->linesize[1];
                    vf.linesize[2] = frame->linesize[2];
                    vf.resolution.width = frame->width;
                    vf.resolution.height = frame->height;
                    vf.format = frame->format;
                    vf.pts = frame->pts;
                    DispatchDecodedVideoFrame(vf);
                }
            }

            // Frame rate control for video (33ms ~ 30fps)
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
        // Process audio packets
        else if (packet->stream_index == audioStreamIndex && audioStreamIndex >= 0) {
            if (audio_decoder_ && audio_renderer_) {
                auto frame = audio_decoder_->Decode(packet);
                if (frame) {
                    // Check if resampling is needed
                    AVSampleFormat srcFormat = static_cast<AVSampleFormat>(frame->format);
                    if (srcFormat != AV_SAMPLE_FMT_S16 && swr_context_) {
                        // Calculate output samples
                        int out_samples = av_rescale_rnd(swr_get_delay(swr_context_, frame->sample_rate) + frame->nb_samples,
                                                         frame->sample_rate, frame->sample_rate, AV_ROUND_UP);
                        int out_channels = frame->ch_layout.nb_channels;
                        size_t out_buffer_size = out_samples * out_channels * sizeof(int16_t);

                        // Resize buffer if needed
                        if (audio_resample_buffer_.size() < out_buffer_size) {
                            audio_resample_buffer_.resize(out_buffer_size);
                        }

                        // Resample - FLTP is planar (separate channels), S16 is interleaved
                        uint8_t* out_ptr = audio_resample_buffer_.data();
                        int converted_samples = swr_convert(swr_context_, &out_ptr, out_samples,
                                                            const_cast<const uint8_t**>(frame->data), frame->nb_samples);
                        if (converted_samples > 0) {
                            int data_size = converted_samples * out_channels * sizeof(int16_t);

                            // Debug: dump audio data BEFORE writing to renderer
                            if (!audio_dump_initialized_) {
                                audio_dump_file_.open("/tmp/audio_dump.pcm", std::ios::binary);
                                audio_dump_initialized_ = true;
                                LOG_INFO("Player", "Audio dump started: /tmp/audio_dump.pcm");
                            }
                            if (audio_dump_file_.is_open()) {
                                audio_dump_file_.write(reinterpret_cast<const char*>(audio_resample_buffer_.data()), data_size);
                            }

                            audio_renderer_->WriteAudio(audio_resample_buffer_.data(), data_size);

                            // Track pre-buffer progress
                            if (!audio_prebuffered) {
                                audio_packets_decoded++;
                            }
                        }
                    } else if (srcFormat == AV_SAMPLE_FMT_S16) {
                        // Already S16, write directly
                        int data_size = av_samples_get_buffer_size(nullptr, frame->ch_layout.nb_channels,
                                                                    frame->nb_samples,
                                                                    static_cast<AVSampleFormat>(frame->format), 1);
                        if (data_size > 0) {
                            audio_renderer_->WriteAudio(frame->data[0], data_size);
                        }
                    } else {
                        LOG_WARNING("Player", "Audio format not supported: " + std::to_string(srcFormat));
                    }

                    // Dispatch for callbacks (original format)
                    int data_size = av_samples_get_buffer_size(nullptr, frame->ch_layout.nb_channels,
                                                                frame->nb_samples,
                                                                static_cast<AVSampleFormat>(frame->format), 1);
                    AudioFrame af;
                    af.data = frame->data[0];
                    af.size = data_size;
                    af.samples = frame->nb_samples;
                    af.format = frame->format;
                    af.sampleRate = frame->sample_rate;
                    af.channels = frame->ch_layout.nb_channels;
                    af.pts = frame->pts;
                    DispatchDecodedAudioFrame(af);
                }
            }

            // For audio-only, use smaller sleep to prevent buffer underrun
            if (videoStreamIndex < 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // Start audio playback after pre-buffering some data
        if (!audio_prebuffered && audio_renderer_ && audio_packets_decoded >= 40) {  // ~1 second of audio
            audio_renderer_->Play();
            audio_prebuffered = true;
            LOG_INFO("Player", "Audio playback started after pre-buffering " +
                     std::to_string(audio_packets_decoded) + " packets");
        }

        // Simple frame rate control (33ms ~ 30fps)
        // std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    // Stop audio playback
    if (audio_renderer_) {
        audio_renderer_->Stop();
    }

    if (state_ == PlayerState::kPlaying) {
        state_ = PlayerState::kStopped;
    }
}

PlayerState PlayerImpl::GetState() const {
    return state_.load();
}

MediaInfo PlayerImpl::GetMediaInfo() const {
    return media_info_;
}

Timestamp PlayerImpl::GetCurrentPosition() const {
    return 0; // TODO: Implement proper position tracking
}

Timestamp PlayerImpl::GetDuration() const {
    return media_info_.duration_ms;
}

std::unique_ptr<IPlayer> CreatePlayer() {
    return std::make_unique<PlayerImpl>();
}

} // namespace avsdk
