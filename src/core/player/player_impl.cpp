#include "player_impl.h"
#include "avsdk/logger.h"

#include <chrono>
#include <thread>

namespace avsdk {

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

    LOG_INFO("Player", "Opened: " + url);
    return ErrorCode::OK;
}

void PlayerImpl::Close() {
    Stop();
    if (demuxer_) {
        demuxer_->Close();
        demuxer_.reset();
    }
    video_decoder_.reset();
    audio_decoder_.reset();
    if (video_renderer_) {
        video_renderer_->Release();
        video_renderer_.reset();
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
    LOG_INFO("Player", "Playback loop started, video stream index: " + std::to_string(videoStreamIndex));

    while (!should_stop_ && state_ == PlayerState::kPlaying) {
        if (!demuxer_) break;

        auto packet = demuxer_->ReadPacket();
        if (!packet) {
            break; // End of stream
        }

        // Only process video packets
        if (packet->stream_index == videoStreamIndex) {
            LOG_INFO("Player", "Processing video packet, pts: " + std::to_string(packet->pts));
            // Decode video and render
            if (video_decoder_ && video_renderer_) {
                auto frame = video_decoder_->Decode(packet);
                if (frame) {
                    LOG_INFO("Player", "Decoded frame " + std::to_string(frame->width) + "x" + std::to_string(frame->height));
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

        // Simple frame rate control (33ms ~ 30fps)
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
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
