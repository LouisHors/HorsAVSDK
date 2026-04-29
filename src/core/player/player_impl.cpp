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
    HLS,
    FLV,
    RTMP,
    Unknown
};

static URLType DetectURLType(const std::string& url) {
    // Check for rtmp://
    if (url.rfind("rtmp://", 0) == 0 || url.rfind("rtmps://", 0) == 0) {
        return URLType::RTMP;
    }
    // Check for HLS by .m3u8 extension
    if (url.find(".m3u8") != std::string::npos) {
        return URLType::HLS;
    }
    // Check for FLV by .flv extension
    if (url.find(".flv") != std::string::npos) {
        return URLType::FLV;
    }
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
    // Clean up any resources from a previous open to prevent memory leaks
    if (audio_renderer_) {
        audio_renderer_->Close();
        audio_renderer_.reset();
    }
    for (auto& decoder : audio_decoders_) {
        if (decoder) {
            decoder->Close();
        }
    }
    audio_decoders_.clear();
    selected_audio_track_ = 0;
    if (video_decoder_) {
        video_decoder_->Close();
        video_decoder_.reset();
    }
    hw_decoder_active_ = false;
    if (swr_context_) {
        swr_free(&swr_context_);
        swr_context_ = nullptr;
    }
    audio_resample_buffer_.clear();
    audio_clock_.Reset();
    first_audio_pts_ = -1.0;
    if (audio_dump_file_.is_open()) {
        audio_dump_file_.close();
    }
    audio_dump_initialized_ = false;
    if (demuxer_) {
        demuxer_->Close();
        demuxer_.reset();
    }
    {
        std::lock_guard<std::mutex> lock(video_queue_mutex_);
        video_frame_queue_.clear();
    }
    playback_finished_ = false;

    URLType urlType = DetectURLType(url);
    switch (urlType) {
        case URLType::HLS:
            LOG_INFO("Player", "Detected HLS stream, using FFmpegDemuxer (FFmpeg built-in HLS support)");
            demuxer_ = CreateFFmpegDemuxer();
            break;
        case URLType::FLV:
            LOG_INFO("Player", "Detected FLV stream, using NetworkDemuxer");
            demuxer_ = CreateNetworkDemuxer();
            break;
        case URLType::RTMP:
            LOG_INFO("Player", "Detected RTMP stream, using FFmpegDemuxer");
            demuxer_ = CreateFFmpegDemuxer();
            break;
        case URLType::HTTP:
        case URLType::HTTPS:
            LOG_INFO("Player", "Detected network URL, using FFmpegDemuxer (HTTP/HTTPS supported by libavformat)");
            demuxer_ = CreateFFmpegDemuxer();
            break;
        case URLType::LocalFile:
        default:
            LOG_INFO("Player", "Detected local file URL, using FFmpegDemuxer");
            demuxer_ = CreateFFmpegDemuxer();
            break;
    }

    auto result = demuxer_->Open(url);
    if (result != ErrorCode::OK) {
        NotifyError(result, "Failed to open URL: " + url);
        return result;
    }

    media_info_ = demuxer_->GetMediaInfo();

    // Get timebases for AV sync
    video_timebase_ = demuxer_->GetVideoTimebase();
    audio_timebases_.clear();
    for (int idx : demuxer_->GetAudioStreamIndices()) {
        audio_timebases_.push_back(demuxer_->GetAudioTimebase(idx));
    }
    LOG_INFO("Player", "Timebases - Video: " + std::to_string(video_timebase_) +
             ", Audio tracks: " + std::to_string(audio_timebases_.size()));

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
                    hw_decoder_active_ = true;
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

    // Initialize audio decoders and renderer
    if (media_info_.has_audio) {
        auto audioIndices = demuxer_->GetAudioStreamIndices();
        audio_decoders_.clear();
        for (size_t i = 0; i < audioIndices.size(); i++) {
            auto decoder = CreateFFmpegDecoder();
            AVCodecParameters* audioCodecpar = demuxer_->GetAudioCodecParameters(audioIndices[i]);
            if (audioCodecpar) {
                auto result = decoder->Initialize(audioCodecpar);
                if (result != ErrorCode::OK) {
                    LOG_ERROR("Player", "Failed to initialize audio decoder for track " + std::to_string(i));
                } else {
                    LOG_INFO("Player", "Audio decoder initialized for track " + std::to_string(i) +
                             " (stream " + std::to_string(audioIndices[i]) + ")");
                }
            }
            audio_decoders_.push_back(std::move(decoder));
        }

        if (!audio_decoders_.empty() && audio_decoders_[0]) {
            // Initialize audio renderer (shared across all tracks)
            audio_renderer_ = CreatePlatformAudioRenderer();
            if (audio_renderer_) {
                AudioFormat audioFormat;
                audioFormat.sample_rate = media_info_.audio_sample_rate;
                audioFormat.channels = media_info_.audio_channels;
                audioFormat.bits_per_sample = 16; // Default to 16-bit
                auto result = audio_renderer_->Initialize(audioFormat);
                if (result != ErrorCode::OK) {
                    LOG_ERROR("Player", "Failed to initialize audio renderer");
                    audio_renderer_.reset();
                } else {
                    LOG_INFO("Player", "Audio renderer initialized: " +
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
    for (auto& decoder : audio_decoders_) {
        decoder.reset();
    }
    audio_decoders_.clear();
    selected_audio_track_ = 0;
    hw_decoder_active_ = false;
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

    // Clear video frame queue
    {
        std::lock_guard<std::mutex> lock(video_queue_mutex_);
        video_frame_queue_.clear();
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
    if (video_render_thread_.joinable()) {
        video_render_thread_.join();
    }

    auto oldState = state_.exchange(PlayerState::kPlaying);
    should_stop_ = false;
    video_render_stop_ = false;
    playback_finished_ = false;
    playback_thread_ = std::thread(&PlayerImpl::PlaybackLoop, this);
    video_render_thread_ = std::thread(&PlayerImpl::VideoRenderLoop, this);

    LOG_INFO("Player", "Started playback (playback + video render threads)");
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
    video_render_stop_ = true;
    video_queue_cv_.notify_all();

    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }
    if (video_render_thread_.joinable()) {
        video_render_thread_.join();
    }

    // Clear video frame queue
    {
        std::lock_guard<std::mutex> lock(video_queue_mutex_);
        video_frame_queue_.clear();
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
    auto audioIndices = demuxer_->GetAudioStreamIndices();
    int activeAudioStreamIndex = (selected_audio_track_ < static_cast<int>(audioIndices.size()))
        ? audioIndices[selected_audio_track_] : -1;
    IDecoder* activeAudioDecoder = (selected_audio_track_ < static_cast<int>(audio_decoders_.size()))
        ? audio_decoders_[selected_audio_track_].get() : nullptr;
    double activeAudioTimebase = (selected_audio_track_ < static_cast<int>(audio_timebases_.size()))
        ? audio_timebases_[selected_audio_track_] : 0.0;
    LOG_INFO("Player", "Playback loop started, video stream index: " + std::to_string(videoStreamIndex) +
             ", active audio stream index: " + std::to_string(activeAudioStreamIndex) +
             " (track " + std::to_string(selected_audio_track_) + " / " + std::to_string(audioIndices.size()) + ")");

    // Pre-buffer audio before starting playback
    bool audio_prebuffered = false;
    int audio_packets_decoded = 0;
    int64_t total_audio_samples = 0;
    int total_packets_read = 0;  // DEBUG

    // Progress tracking
    Timestamp last_progress_report = 0;
    const Timestamp progress_interval_ms = 500; // Report every 500ms

    // Buffer flow control - keep audio buffer small to match video queue latency (~333ms)
    const int kMinBufferMs = 100;    // Minimum: 100ms
    const int kMaxBufferMs = 350;    // Maximum: 350ms (match video queue 10 frames @ 30fps ≈ 333ms)

    // Start audio clock
    audio_clock_.Reset();

    while (!should_stop_ && state_ == PlayerState::kPlaying) {
        if (!demuxer_) break;

        // Non-blocking audio flow control: if buffer is very full, give AudioUnit
        // a chance to drain by yielding briefly. This avoids losing audio data
        // without blocking video decoding for long periods.
        if (audio_renderer_ && audio_prebuffered && activeAudioStreamIndex >= 0) {
            int buffered_ms = audio_renderer_->GetBufferedDuration();
            if (buffered_ms > kMaxBufferMs) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        auto packet = demuxer_->ReadPacket();
        total_packets_read++;  // DEBUG
        if (!packet) {
            LOG_INFO("Player", "Total packets read: " + std::to_string(total_packets_read));  // DEBUG

            // ---- Drain decoder internal buffers ----
            // FFmpeg decoders buffer frames internally; send nullptr to flush them out.
            if (video_decoder_ && videoStreamIndex >= 0) {
                LOG_INFO("Player", "Draining video decoder...");
                int drained = 0;
                while (!should_stop_) {
                    auto frame = video_decoder_->Decode(nullptr);
                    if (!frame) break;
                    drained++;
                    double frame_pts = frame->pts * video_timebase_;
                    {
                        std::unique_lock<std::mutex> lock(video_queue_mutex_);
                        video_queue_cv_.wait(lock, [this] {
                            return video_frame_queue_.size() < kMaxVideoQueueSize || should_stop_;
                        });
                        if (should_stop_) break;
                        VideoFrameItem item;
                        item.frame = frame;
                        item.pts = frame_pts;
                        video_frame_queue_.push_back(std::move(item));
                    }
                    video_queue_cv_.notify_one();
                }
                LOG_INFO("Player", "Drained " + std::to_string(drained) + " video frames from decoder");
            }

            if (activeAudioDecoder && activeAudioStreamIndex >= 0) {
                LOG_INFO("Player", "Draining audio decoder...");
                int drained = 0;
                while (!should_stop_) {
                    auto frame = activeAudioDecoder->Decode(nullptr);
                    if (!frame) break;
                    drained++;
                    // Process drained audio frame same as normal decode path
                    AVSampleFormat srcFormat = static_cast<AVSampleFormat>(frame->format);
                    if (srcFormat != AV_SAMPLE_FMT_S16 && swr_context_) {
                        int out_samples = av_rescale_rnd(swr_get_delay(swr_context_, frame->sample_rate) + frame->nb_samples,
                                                         frame->sample_rate, frame->sample_rate, AV_ROUND_UP);
                        int out_channels = frame->ch_layout.nb_channels;
                        size_t out_buffer_size = out_samples * out_channels * sizeof(int16_t);
                        if (audio_resample_buffer_.size() < out_buffer_size) {
                            audio_resample_buffer_.resize(out_buffer_size);
                        }
                        uint8_t* out_ptr = audio_resample_buffer_.data();
                        int converted_samples = swr_convert(swr_context_, &out_ptr, out_samples,
                                                            const_cast<const uint8_t**>(frame->data), frame->nb_samples);
                        if (converted_samples > 0) {
                            int data_size = converted_samples * out_channels * sizeof(int16_t);
                            int written = 0, retries = 0;
                            while (written < data_size && retries < 20) {
                                int n = audio_renderer_->WriteAudio(audio_resample_buffer_.data() + written, data_size - written);
                                written += n;
                                if (written < data_size) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                                    retries++;
                                }
                            }
                            if (audio_prebuffered) {
                                audio_clock_.UpdateBySamples(converted_samples, frame->sample_rate);
                            }
                        }
                    } else if (srcFormat == AV_SAMPLE_FMT_S16) {
                        int data_size = av_samples_get_buffer_size(nullptr, frame->ch_layout.nb_channels,
                                                                    frame->nb_samples,
                                                                    static_cast<AVSampleFormat>(frame->format), 1);
                        if (data_size > 0) {
                            int written = 0, retries = 0;
                            while (written < data_size && retries < 20) {
                                int n = audio_renderer_->WriteAudio(frame->data[0] + written, data_size - written);
                                written += n;
                                if (written < data_size) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                                    retries++;
                                }
                            }
                            if (audio_prebuffered) {
                                audio_clock_.UpdateBySamples(frame->nb_samples, frame->sample_rate);
                            }
                        }
                    }
                }
                LOG_INFO("Player", "Drained " + std::to_string(drained) + " audio frames from decoder");
            }

            // Mark playback as finished so RenderLoop can drain queue and exit
            playback_finished_ = true;
            video_queue_cv_.notify_all();

            // Wait for video queue to drain before stopping audio
            {
                std::unique_lock<std::mutex> lock(video_queue_mutex_);
                int wait_count = 0;
                while (!video_frame_queue_.empty() && !should_stop_ && wait_count < 2000) {
                    LOG_INFO("Player", "Waiting for video queue to drain, remaining: " +
                             std::to_string(video_frame_queue_.size()));
                    video_queue_cv_.wait_for(lock, std::chrono::milliseconds(10));
                    wait_count++;
                }
            }

            // End of stream reached - wait for audio buffer to drain
            if (audio_renderer_ && activeAudioStreamIndex >= 0) {
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

            // Final progress update so UI shows 100%
            NotifyProgress(GetDuration(), GetDuration());
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
                if (!frame && hw_decoder_active_) {
                    LOG_WARNING("Player", "Hardware decoder returned null, falling back to software decoder");
                    video_decoder_.reset();
                    video_decoder_ = CreateFFmpegDecoder();
                    auto sw_result = video_decoder_->Initialize(demuxer_->GetVideoCodecParameters());
                    if (sw_result == ErrorCode::OK) {
                        hw_decoder_active_ = false;
                        frame = video_decoder_->Decode(packet);
                    } else {
                        LOG_ERROR("Player", "Failed to initialize software decoder fallback");
                    }
                }

                if (frame) {
                    // Enqueue decoded frame for video render thread
                    double frame_pts = frame->pts * video_timebase_;

                    {
                        std::unique_lock<std::mutex> lock(video_queue_mutex_);
                        // Block when queue is full instead of dropping frames.
                        // This naturally throttles PlaybackLoop to real-time speed.
                        if (video_frame_queue_.size() >= kMaxVideoQueueSize) {
                            LOG_INFO("Player", "Video queue full (" + std::to_string(video_frame_queue_.size()) +
                                     "), throttling decode until RenderLoop catches up");
                        }
                        video_queue_cv_.wait(lock, [this] {
                            return video_frame_queue_.size() < kMaxVideoQueueSize || should_stop_;
                        });
                        if (should_stop_) break;

                        VideoFrameItem item;
                        item.frame = frame;
                        item.pts = frame_pts;
                        video_frame_queue_.push_back(std::move(item));
                        LOG_INFO("Player", "Enqueued video frame PTS=" + std::to_string(frame_pts) +
                                 "s, queue_size=" + std::to_string(video_frame_queue_.size()));
                    }
                    video_queue_cv_.notify_one();

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
        else if (packet->stream_index == activeAudioStreamIndex && activeAudioStreamIndex >= 0) {
            if (activeAudioDecoder && audio_renderer_) {
                auto frame = activeAudioDecoder->Decode(packet);
                if (frame) {
                    // Calculate audio PTS
                    double audio_pts = frame->pts * activeAudioTimebase;
                    if (first_audio_pts_ < 0) {
                        first_audio_pts_ = audio_pts;
                        LOG_INFO("Player", "First audio PTS captured: " + std::to_string(first_audio_pts_) + "s");
                    }

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
                            int written = 0;
                            int retries = 0;
                            while (written < data_size && retries < 20) {
                                int n = audio_renderer_->WriteAudio(audio_resample_buffer_.data() + written, data_size - written);
                                written += n;
                                if (written < data_size) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                                    retries++;
                                }
                            }

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
                            int written = 0;
                            int retries = 0;
                            while (written < data_size && retries < 20) {
                                int n = audio_renderer_->WriteAudio(frame->data[0] + written, data_size - written);
                                written += n;
                                if (written < data_size) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                                    retries++;
                                }
                            }

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
        if (!audio_prebuffered && audio_renderer_ && audio_packets_decoded >= 10) {  // ~250ms of audio
            // Wait until we have at least 300ms of buffered audio before starting
            int buffered_ms = audio_renderer_->GetBufferedDuration();
            if (buffered_ms < 300) {
                continue;
            }

            // Initialize audio clock with first audio PTS (compensates for streams not starting at 0)
            double clock_base = (first_audio_pts_ >= 0) ? first_audio_pts_ : 0.0;
            audio_clock_.UpdateByPTS(clock_base);
            audio_renderer_->Play();
            audio_clock_.Start();
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

void PlayerImpl::VideoRenderLoop() {
    LOG_INFO("Player", "Video render thread started");

    // Pace according to the source video frame rate, not a hardcoded 30fps
    double fps = media_info_.video_framerate;
    if (fps <= 0) fps = 30.0;
    auto frame_interval = std::chrono::milliseconds(static_cast<int>(1000.0 / fps));
    LOG_INFO("Player", "Video render pacing: " + std::to_string(fps) + " fps, interval=" +
             std::to_string(frame_interval.count()) + "ms");

    auto next_render_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    auto last_log_time = std::chrono::steady_clock::now();

    // Loose AV drift correction state
    double last_check_audio_time = 0.0;
    double last_check_video_pts = 0.0;
    int drift_check_interval = 30;  // Check every ~1 second (30 frames)

    while (!video_render_stop_ && !should_stop_) {
        // Wait for audio clock to start so video doesn't race ahead of audio
        if (!audio_clock_.IsRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // ---- 1. Timer-driven: sleep until next frame slot ----
        auto now = std::chrono::steady_clock::now();
        if (now < next_render_time) {
            std::this_thread::sleep_for(next_render_time - now);
        }

        // ---- 2. Dequeue a frame ----
        VideoFrameItem item;
        bool got_frame = false;
        {
            std::unique_lock<std::mutex> lock(video_queue_mutex_);
            if (video_queue_cv_.wait_for(lock, std::chrono::milliseconds(50), [this] {
                return !video_frame_queue_.empty() || video_render_stop_ || should_stop_;
            })) {
                if (video_render_stop_ || should_stop_) break;
                if (!video_frame_queue_.empty()) {
                    item = std::move(video_frame_queue_.front());
                    video_frame_queue_.erase(video_frame_queue_.begin());
                    got_frame = true;
                }
            }
        }
        if (got_frame) {
            video_queue_cv_.notify_one();
        }

        // No frame available - check if playback finished, otherwise retry
        if (!got_frame) {
            if (playback_finished_.load()) {
                LOG_INFO("Player", "RenderLoop: playback finished, queue empty, exiting");
                break;
            }
            LOG_INFO("Player", "RenderLoop: no frame available, queue empty");
            next_render_time += frame_interval;
            continue;
        }

        // ---- 3. Render ----
        frame_count++;
        auto render_now = std::chrono::steady_clock::now();
        double actual_interval = std::chrono::duration<double>(render_now - (next_render_time - frame_interval)).count();

        // Log AV sync offset on every frame for debugging
        double audio_now = audio_clock_.GetTime();
        double av_offset_ms = (item.pts - audio_now) * 1000.0;  // positive = video ahead
        if (frame_count <= 5 || frame_count % 30 == 0) {
            LOG_INFO("Player", "RenderLoop: frame=" + std::to_string(frame_count) +
                     " video_pts=" + std::to_string(item.pts) +
                     "s audio_clock=" + std::to_string(audio_now) +
                     "s av_offset=" + std::to_string(av_offset_ms) + "ms");
        }
        RenderVideoFrame(item.frame.get());

        // ---- 4. Loose AV drift correction ----
        // Compare video PTS delta vs audio clock delta every N frames.
        // If they diverge beyond threshold, gently adjust next_render_time.
        // Skip correction when playback is finishing to avoid acting on a stopped clock.
        if (!playback_finished_.load() && frame_count % drift_check_interval == 0) {
            double audio_now = audio_clock_.GetTime();
            double video_now = item.pts;
            if (last_check_audio_time > 0.0 && audio_now > 0.0) {
                double audio_delta = audio_now - last_check_audio_time;
                double video_delta = video_now - last_check_video_pts;
                double drift = video_delta - audio_delta;  // positive = video is ahead

                if (std::abs(drift) > 0.05) {  // 50ms threshold
                    // Spread correction over next interval to avoid jarring jumps
                    auto adjustment = std::chrono::microseconds(
                        static_cast<int>((drift / drift_check_interval) * 1000000));
                    next_render_time -= adjustment;  // delay if video ahead, advance if behind
                    LOG_INFO("Player", "AV drift correction: drift=" + std::to_string(drift * 1000.0) +
                             "ms, per-frame adjustment=" + std::to_string(adjustment.count()) + "us");
                }
            }
            last_check_audio_time = audio_now;
            last_check_video_pts = video_now;
        }

        // Log every 30 frames (~1 second) or on significant events
        if (frame_count % 30 == 1) {
            auto elapsed = std::chrono::duration<double>(render_now - last_log_time).count();
            LOG_INFO("Player", "RenderLoop: rendered frame " + std::to_string(frame_count) +
                     " PTS=" + std::to_string(item.pts) + "s, actual_interval=" +
                     std::to_string(actual_interval * 1000.0) + "ms");
            last_log_time = render_now;
        }

        next_render_time += frame_interval;
    }

    LOG_INFO("Player", "Video render thread stopped, total frames rendered=" + std::to_string(frame_count));
}

PlayerState PlayerImpl::GetState() const {
    return state_.load();
}

MediaInfo PlayerImpl::GetMediaInfo() const {
    return media_info_;
}

Timestamp PlayerImpl::GetCurrentPosition() const {
    // Use audio clock for position tracking (audio is master clock)
    return static_cast<Timestamp>(audio_clock_.GetTimeMs());
}

Timestamp PlayerImpl::GetDuration() const {
    return media_info_.duration_ms;
}

std::vector<AudioTrackInfo> PlayerImpl::GetAudioTracks() const {
    return media_info_.audio_tracks;
}

ErrorCode PlayerImpl::SelectAudioTrack(int trackIndex) {
    auto audioIndices = demuxer_->GetAudioStreamIndices();
    if (trackIndex < 0 || trackIndex >= static_cast<int>(audioIndices.size())) {
        LOG_ERROR("Player", "Invalid audio track index: " + std::to_string(trackIndex) +
                  ", total tracks: " + std::to_string(audioIndices.size()));
        return ErrorCode::InvalidParameter;
    }
    if (trackIndex == selected_audio_track_) {
        return ErrorCode::OK;
    }
    LOG_INFO("Player", "Switching audio track from " + std::to_string(selected_audio_track_) +
             " to " + std::to_string(trackIndex));
    selected_audio_track_ = trackIndex;
    if (selected_audio_track_ < static_cast<int>(audio_decoders_.size()) && audio_decoders_[selected_audio_track_]) {
        audio_decoders_[selected_audio_track_]->Flush();
    }
    return ErrorCode::OK;
}

std::unique_ptr<IPlayer> CreatePlayer() {
    return std::make_unique<PlayerImpl>();
}

} // namespace avsdk
