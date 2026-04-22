#include "player_impl.h"
#include "avsdk/logger.h"
#include "core/demuxer/ffmpeg_demuxer.h"
#include "core/demuxer/network_demuxer.h"

#include <chrono>
#include <thread>
#include <fstream>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

// Platform audio renderer factory
#if defined(__APPLE__)
#include "platform/macos/audio_unit_renderer.h"
#include "platform/macos/videotoolbox_decoder.h"
#endif

namespace avsdk {

// Helper to detect URL type
enum class URLType {
    LocalFile,
    HTTP,
    HTTPS,
    Unknown
};

static URLType DetectURLType(const std::string& url) {
    // Check for http:// or https://
    if (url.rfind("http://", 0) == 0) {
        return URLType::HTTP;
    }
    if (url.rfind("https://", 0) == 0) {
        return URLType::HTTPS;
    }
    // Check for file://
    if (url.rfind("file://", 0) == 0) {
        return URLType::LocalFile;
    }
    // If no protocol, assume local file
    return URLType::LocalFile;
}

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
    auto oldState = state_.exchange(PlayerState::kStopped);
    LOG_INFO("Player", "Initialized");
    NotifyStateChanged(oldState, PlayerState::kStopped);
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

void PlayerImpl::SetCallback(IPlayerCallback* callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = callback;
}

void PlayerImpl::NotifyStateChanged(PlayerState oldState, PlayerState newState) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->OnStateChanged(oldState, newState);
    }
}

void PlayerImpl::NotifyError(ErrorCode error, const std::string& message) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->OnError(error, message);
    }
}

void PlayerImpl::NotifyPrepared() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->OnPrepared(media_info_);
    }
}

void PlayerImpl::NotifyComplete() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->OnComplete();
    }
}

void PlayerImpl::NotifyProgress(Timestamp position, Timestamp duration) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->OnProgress(position, duration);
    }
}

void PlayerImpl::NotifySeekComplete(Timestamp position) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->OnSeekComplete(position);
    }
}

void PlayerImpl::NotifyBuffering(bool isBuffering, int percent) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->OnBuffering(isBuffering, percent);
    }
}

ErrorCode PlayerImpl::Open(const std::string& url) {
    URLType urlType = DetectURLType(url);
    if (urlType == URLType::HTTP || urlType == URLType::HTTPS) {
        LOG_INFO("Player", "Detected network URL, using NetworkDemuxer");
        demuxer_ = CreateNetworkDemuxer();
    } else {
        LOG_INFO("Player", "Detected local file URL, using FFmpegDemuxer");
        demuxer_ = CreateFFmpegDemuxer();
    }

    auto result = demuxer_->Open(url);
    if (result != ErrorCode::OK) {
        NotifyError(result, "Failed to open URL: " + url);
        return result;
    }

    media_info_ = demuxer_->GetMediaInfo();

    // Get timebases for AV sync
    video_timebase_ = demuxer_->GetVideoTimebase();
    audio_timebase_ = demuxer_->GetAudioTimebase();
    LOG_INFO("Player", "Timebases - Video: " + std::to_string(video_timebase_) +
             ", Audio: " + std::to_string(audio_timebase_));

    // Initialize decoders
    if (media_info_.has_video) {
        AVCodecParameters* codecpar = demuxer_->GetVideoCodecParameters();
        if (!codecpar) {
            LOG_ERROR("Player", "Failed to get video codec parameters");
            return ErrorCode::CodecNotFound;
        }

        // Try hardware decoder first if enabled
        bool hw_decoder_initialized = false;
        if (config_.enable_hardware_decoder) {
#if defined(__APPLE__)
            if (IsHardwareDecoderAvailable(codecpar->codec_id)) {
                LOG_INFO("Player", "Trying VideoToolbox hardware decoder");
                video_decoder_ = CreateVideoToolboxDecoder();
                auto hw_result = video_decoder_->Initialize(codecpar);
                if (hw_result == ErrorCode::OK) {
                    LOG_INFO("Player", "VideoToolbox decoder initialized successfully");
                    hw_decoder_initialized = true;
                    // Mark that we have a hardware frame for renderer
                    // The renderer will check frame format to detect this
                } else {
                    LOG_WARNING("Player", "VideoToolbox decoder failed, falling back to software");
                    video_decoder_.reset();
                }
            }
#endif
        }

        // Fall back to software decoder
        if (!hw_decoder_initialized) {
            video_decoder_ = CreateFFmpegDecoder();
            auto result = video_decoder_->Initialize(codecpar);
            if (result != ErrorCode::OK) {
                LOG_ERROR("Player", "Failed to initialize video decoder");
                return result;
            }
            LOG_INFO("Player", "Software decoder initialized");
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
    NotifyPrepared();
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

    // Ensure any previous thread is properly cleaned up before starting a new one
    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }

    auto oldState = state_.exchange(PlayerState::kPlaying);
    should_stop_ = false;
    playback_thread_ = std::thread(&PlayerImpl::PlaybackLoop, this);

    LOG_INFO("Player", "Started playback");
    NotifyStateChanged(oldState, PlayerState::kPlaying);
    return ErrorCode::OK;
}

ErrorCode PlayerImpl::Pause() {
    auto oldState = state_.exchange(PlayerState::kPaused);
    LOG_INFO("Player", "Paused");
    NotifyStateChanged(oldState, PlayerState::kPaused);
    return ErrorCode::OK;
}

ErrorCode PlayerImpl::Stop() {
    auto oldState = state_.exchange(PlayerState::kStopped);
    should_stop_ = true;

    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }

    LOG_INFO("Player", "Stopped");
    NotifyStateChanged(oldState, PlayerState::kStopped);
    return ErrorCode::OK;
}

ErrorCode PlayerImpl::Seek(Timestamp position_ms) {
    if (!demuxer_) return ErrorCode::NotInitialized;
    auto result = demuxer_->Seek(position_ms);
    if (result == ErrorCode::OK) {
        // Reset audio clock after seek
        audio_clock_.Reset();
        audio_clock_.UpdateByPTS(position_ms / 1000.0);
        NotifySeekComplete(position_ms);
    }
    return result;
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
    bool audio_prebuffered = false;
    int audio_packets_decoded = 0;
    int64_t total_audio_samples = 0;
    int total_packets_read = 0;  // DEBUG

    // Progress tracking
    Timestamp last_progress_report = 0;
    const Timestamp progress_interval_ms = 500; // Report every 500ms

    // Buffer flow control - maintain 2-5 seconds of buffered audio
    const int kMinBufferMs = 1000;   // Minimum: 1 second (need to read more)
    const int kMaxBufferMs = 5000;   // Maximum: 5 seconds (pause reading)
    const int kTargetBufferMs = 3000; // Target: 3 seconds

    // Start audio clock
    audio_clock_.Reset();

    while (!should_stop_ && state_ == PlayerState::kPlaying) {
        if (!demuxer_) break;

        // Flow control: Check audio buffer level before reading next packet
        // This implements the "circular buffer read/write" strategy
        if (audio_renderer_ && audio_prebuffered && audioStreamIndex >= 0) {
            int buffered_ms = audio_renderer_->GetBufferedDuration();

            if (buffered_ms > kMaxBufferMs) {
                // Buffer is full - wait for consumer to drain
                LOG_INFO("Player", "Buffer full (" + std::to_string(buffered_ms) +
                         "ms), pausing read to let playback consume...");
                int wait_count = 0;
                while (wait_count < 100 && !should_stop_) {  // Max 1 second wait
                    buffered_ms = audio_renderer_->GetBufferedDuration();
                    if (buffered_ms <= kTargetBufferMs) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    wait_count++;
                }
            }
        }

        auto packet = demuxer_->ReadPacket();
        total_packets_read++;  // DEBUG
        if (!packet) {
            LOG_INFO("Player", "Total packets read: " + std::to_string(total_packets_read));  // DEBUG
            // End of stream reached - wait for audio buffer to drain
            if (audio_renderer_ && audioStreamIndex >= 0) {
                LOG_INFO("Player", "End of stream, waiting for audio buffer to drain...");
                int wait_count = 0;
                while (wait_count < 2000 && !should_stop_) {  // Max wait 20 seconds for long audio
                    int buffered_ms = audio_renderer_->GetBufferedDuration();
                    if (buffered_ms <= 200) {  // Less than 200ms remaining
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    wait_count++;
                }
            }
            NotifyComplete();
            break;
        }

        // Report progress periodically
        Timestamp current_pos = GetCurrentPosition();
        if (current_pos - last_progress_report >= progress_interval_ms) {
            NotifyProgress(current_pos, media_info_.duration_ms);
            last_progress_report = current_pos;
        }

        // Process video packets
        if (packet->stream_index == videoStreamIndex && videoStreamIndex >= 0) {
            // Decode video and render with sync
            if (video_decoder_ && video_renderer_) {
                auto frame = video_decoder_->Decode(packet);
                if (frame) {
                    // Calculate PTS in seconds
                    double frame_pts = frame->pts * video_timebase_;

                    // Wait for sync if audio is playing (audio is master clock)
                    if (audioStreamIndex >= 0 && audio_prebuffered) {
                        int64_t delay_ms = audio_clock_.GetVideoDelayMs(frame_pts);

                        // If frame is early, wait
                        if (delay_ms > 5) {
                            if (delay_ms > 100) {
                                // Cap max wait to prevent long stalls
                                delay_ms = 100;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                        }
                        // If frame is very late (>100ms), skip it
                        else if (delay_ms < -100) {
                            LOG_INFO("Player", "Skipping late video frame, PTS: " + std::to_string(frame_pts) +
                                     ", delay: " + std::to_string(delay_ms) + "ms");
                            continue;
                        }
                    } else if (audioStreamIndex < 0) {
                        // No audio - use simple frame rate control
                        std::this_thread::sleep_for(std::chrono::milliseconds(33));
                    }

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
        }
        // Process audio packets
        else if (packet->stream_index == audioStreamIndex && audioStreamIndex >= 0) {
            if (audio_decoder_ && audio_renderer_) {
                auto frame = audio_decoder_->Decode(packet);
                if (frame) {
                    // Calculate audio PTS
                    double audio_pts = frame->pts * audio_timebase_;

                    // Check if resampling is needed
                    AVSampleFormat srcFormat = static_cast<AVSampleFormat>(frame->format);
                    LOG_INFO("Player", "Audio frame format: " + std::to_string(srcFormat) +
                             ", channels: " + std::to_string(frame->ch_layout.nb_channels) +
                             ", samples: " + std::to_string(frame->nb_samples));
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
                            int written = audio_renderer_->WriteAudio(audio_resample_buffer_.data(), data_size);

                            // Update audio clock
                            if (audio_prebuffered) {
                                total_audio_samples += converted_samples;
                                audio_clock_.UpdateBySamples(converted_samples, frame->sample_rate);
                            }

                            // Track pre-buffer progress
                            if (!audio_prebuffered) {
                                audio_packets_decoded++;
                                total_audio_samples += converted_samples;
                            }
                        } else if (converted_samples < 0) {
                            char errbuf[256];
                            av_strerror(converted_samples, errbuf, sizeof(errbuf));
                            LOG_ERROR("Player", "swr_convert failed: " + std::string(errbuf));
                        } else {
                            LOG_WARNING("Player", "swr_convert returned 0 samples");
                        }
                    } else if (srcFormat == AV_SAMPLE_FMT_S16) {
                        // Already S16, write directly
                        int data_size = av_samples_get_buffer_size(nullptr, frame->ch_layout.nb_channels,
                                                                    frame->nb_samples,
                                                                    static_cast<AVSampleFormat>(frame->format), 1);
                        if (data_size > 0) {
                            int written = audio_renderer_->WriteAudio(frame->data[0], data_size);

                            // Update audio clock
                            if (audio_prebuffered) {
                                total_audio_samples += frame->nb_samples;
                                audio_clock_.UpdateBySamples(frame->nb_samples, frame->sample_rate);
                            }

                            if (!audio_prebuffered) {
                                audio_packets_decoded++;
                                total_audio_samples += frame->nb_samples;
                            }
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
        }

        // Start audio playback after pre-buffering some data
        if (!audio_prebuffered && audio_renderer_ && audio_packets_decoded >= 40) {  // ~1 second of audio
            // Wait until we have at least 1 second of buffered audio before starting
            int buffered_ms = audio_renderer_->GetBufferedDuration();
            if (buffered_ms < 1000) {
                // Not enough data yet, continue buffering
                continue;
            }

            // Initialize audio clock with first audio PTS
            audio_clock_.UpdateByPTS(0.0);  // Start from 0
            audio_clock_.Start();
            audio_renderer_->Play();
            audio_prebuffered = true;
            LOG_INFO("Player", "Audio playback started after pre-buffering " +
                     std::to_string(audio_packets_decoded) + " packets, samples: " + std::to_string(total_audio_samples) +
                     ", buffered: " + std::to_string(buffered_ms) + "ms");
        }
    }

    // Stop audio playback
    if (audio_renderer_) {
        audio_renderer_->Stop();
    }

    // Stop audio clock
    audio_clock_.Stop();

    auto oldState = state_.exchange(PlayerState::kStopped);
    if (oldState != PlayerState::kStopped) {
        NotifyStateChanged(oldState, PlayerState::kStopped);
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
