#include "filter_processor.h"
#include "avsdk/logger.h"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
}

namespace avsdk {

FilterProcessor::FilterProcessor()
    : videoGraph_(nullptr)
    , videoSrcCtx_(nullptr)
    , videoSinkCtx_(nullptr)
    , audioGraph_(nullptr)
    , audioSrcCtx_(nullptr)
    , audioSinkCtx_(nullptr)
    , videoInitialized_(false)
    , audioInitialized_(false)
    , videoWidth_(0)
    , videoHeight_(0)
    , videoFormat_(0)
    , audioSampleRate_(0)
    , audioChannels_(0)
    , audioChannelLayout_(0) {
}

FilterProcessor::~FilterProcessor() {
    Release();
}

int FilterProcessor::Initialize(const std::string& videoFilterDesc,
                                const std::string& audioFilterDesc) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Store filter descriptions for lazy initialization
    videoFilterDesc_ = videoFilterDesc;
    audioFilterDesc_ = audioFilterDesc;

    // Reset initialization flags - actual initialization happens on first frame
    videoInitialized_ = false;
    audioInitialized_ = false;

    return 0;
}

bool FilterProcessor::IsInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Consider initialized if we have filter descriptions stored
    return !videoFilterDesc_.empty() || !audioFilterDesc_.empty();
}

void FilterProcessor::Release() {
    std::lock_guard<std::mutex> lock(mutex_);
    Cleanup();
    videoFilterDesc_.clear();
    audioFilterDesc_.clear();
    videoInitialized_ = false;
    audioInitialized_ = false;
}

int FilterProcessor::ProcessVideoFrame(VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (videoFilterDesc_.empty()) {
        // No video filter configured, pass through
        return 0;
    }

    // Lazy initialization on first frame with known format
    if (!videoInitialized_) {
        int ret = InitVideoFilterGraph(frame.resolution.width,
                                       frame.resolution.height,
                                       frame.format);
        if (ret < 0) {
            LOG_ERROR("FilterProcessor", "Failed to initialize video filter graph");
            return ret;
        }
        videoInitialized_ = true;
    }

    // Check if format changed (would need re-initialization)
    if (frame.resolution.width != videoWidth_ ||
        frame.resolution.height != videoHeight_ ||
        frame.format != videoFormat_) {
        LOG_WARNING("FilterProcessor", "Video format changed, reinitializing filter graph");

        // Cleanup and reinitialize
        if (videoGraph_) {
            avfilter_graph_free(&videoGraph_);
            videoGraph_ = nullptr;
            videoSrcCtx_ = nullptr;
            videoSinkCtx_ = nullptr;
        }

        int ret = InitVideoFilterGraph(frame.resolution.width,
                                       frame.resolution.height,
                                       frame.format);
        if (ret < 0) {
            LOG_ERROR("FilterProcessor", "Failed to reinitialize video filter graph");
            return ret;
        }
    }

    if (!videoGraph_ || !videoSrcCtx_ || !videoSinkCtx_) {
        return -1;
    }

    // Convert VideoFrame to AVFrame
    AVFrame* srcFrame = VideoFrameToAVFrame(frame);
    if (!srcFrame) {
        LOG_ERROR("FilterProcessor", "Failed to convert VideoFrame to AVFrame");
        return -1;
    }

    // Push frame into filter graph
    int ret = av_buffersrc_add_frame_flags(videoSrcCtx_, srcFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
        LOG_ERROR("FilterProcessor", "Failed to add frame to buffer source");
        av_frame_free(&srcFrame);
        return ret;
    }

    // Pull filtered frame from filter graph
    AVFrame* dstFrame = av_frame_alloc();
    if (!dstFrame) {
        LOG_ERROR("FilterProcessor", "Failed to allocate output frame");
        av_frame_free(&srcFrame);
        return -1;
    }

    ret = av_buffersink_get_frame(videoSinkCtx_, dstFrame);
    if (ret < 0) {
        LOG_ERROR("FilterProcessor", "Failed to get frame from buffer sink");
        av_frame_free(&dstFrame);
        av_frame_free(&srcFrame);
        return ret;
    }

    // Convert AVFrame back to VideoFrame
    ret = AVFrameToVideoFrame(dstFrame, frame);

    // Cleanup
    av_frame_free(&dstFrame);
    av_frame_free(&srcFrame);

    return ret;
}

int FilterProcessor::ProcessAudioFrame(AudioFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (audioFilterDesc_.empty()) {
        // No audio filter configured, pass through
        return 0;
    }

    // For now, audio filtering is not fully implemented
    // Would follow same pattern as video
    LOG_WARNING("FilterProcessor", "Audio filtering not yet implemented");
    return 0;
}

void FilterProcessor::OnRawPacket(const EncodedPacket& /*packet*/) {
    // Not used for filter processing
}

void FilterProcessor::OnDecodedVideoFrame(const VideoFrame& /*frame*/) {
    // Not used - we process on pre-render
}

void FilterProcessor::OnDecodedAudioFrame(const AudioFrame& /*frame*/) {
    // Not used - we process on pre-render
}

void FilterProcessor::OnPreRenderVideoFrame(VideoFrame& frame) {
    // Apply video filters before rendering
    ProcessVideoFrame(frame);
}

void FilterProcessor::OnPreRenderAudioFrame(AudioFrame& frame) {
    // Apply audio filters before rendering
    ProcessAudioFrame(frame);
}

void FilterProcessor::OnEncodedPacket(const EncodedPacket& /*packet*/) {
    // Not used for filter processing
}

std::shared_ptr<FilterProcessor> FilterProcessor::Create() {
    return std::make_shared<FilterProcessor>();
}

int FilterProcessor::InitVideoFilterGraph(int width, int height, int format) {
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    char args[512];
    int ret;
    AVRational time_base;
    AVRational sar;
    enum AVPixelFormat pix_fmts[2];

    // Store format info
    videoWidth_ = width;
    videoHeight_ = height;
    videoFormat_ = format;

    // Create filter graph
    videoGraph_ = avfilter_graph_alloc();
    if (!outputs || !inputs || !videoGraph_) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    // Prepare buffer source arguments (input format)
    time_base = {1, 30};  // Default 30fps
    sar = {1, 1};         // Default 1:1 sample aspect ratio

    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             width, height, format,
             time_base.num, time_base.den,
             sar.num, sar.den);

    // Create buffer source
    ret = avfilter_graph_create_filter(&videoSrcCtx_, buffersrc, "in",
                                       args, nullptr, videoGraph_);
    if (ret < 0) {
        LOG_ERROR("FilterProcessor", "Failed to create buffer source");
        goto end;
    }

    // Create buffer sink
    ret = avfilter_graph_create_filter(&videoSinkCtx_, buffersink, "out",
                                       nullptr, nullptr, videoGraph_);
    if (ret < 0) {
        LOG_ERROR("FilterProcessor", "Failed to create buffer sink");
        goto end;
    }

    // Set output pixel formats for buffer sink (accept any)
    pix_fmts[0] = (AVPixelFormat)format;
    pix_fmts[1] = AV_PIX_FMT_NONE;
    ret = av_opt_set_int_list(videoSinkCtx_, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        LOG_ERROR("FilterProcessor", "Failed to set output pixel formats");
        goto end;
    }

    // Set up inputs/outputs for filter graph
    outputs->name = av_strdup("in");
    outputs->filter_ctx = videoSrcCtx_;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = videoSinkCtx_;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // Parse filter description and build graph
    ret = avfilter_graph_parse_ptr(videoGraph_, videoFilterDesc_.c_str(),
                                   &inputs, &outputs, nullptr);
    if (ret < 0) {
        LOG_ERROR("FilterProcessor", "Failed to parse filter graph: " + videoFilterDesc_);
        goto end;
    }

    // Configure the filter graph
    ret = avfilter_graph_config(videoGraph_, nullptr);
    if (ret < 0) {
        LOG_ERROR("FilterProcessor", "Failed to configure filter graph");
        goto end;
    }

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    if (ret < 0 && videoGraph_) {
        avfilter_graph_free(&videoGraph_);
        videoGraph_ = nullptr;
        videoSrcCtx_ = nullptr;
        videoSinkCtx_ = nullptr;
    }

    return ret;
}

int FilterProcessor::InitAudioFilterGraph(int sampleRate, int channels, uint64_t channelLayout) {
    // TODO: Implement audio filter graph initialization
    // Similar pattern to video but with abuffer and abuffersink
    audioSampleRate_ = sampleRate;
    audioChannels_ = channels;
    audioChannelLayout_ = channelLayout;
    audioInitialized_ = true;
    return 0;
}

AVFrame* FilterProcessor::VideoFrameToAVFrame(const VideoFrame& frame) {
    AVFrame* avFrame = av_frame_alloc();
    if (!avFrame) {
        return nullptr;
    }

    avFrame->width = frame.resolution.width;
    avFrame->height = frame.resolution.height;
    avFrame->format = frame.format;
    avFrame->pts = frame.pts;

    // Copy data pointers (no copy of actual data)
    for (int i = 0; i < 4; i++) {
        avFrame->data[i] = frame.data[i];
        avFrame->linesize[i] = frame.linesize[i];
    }

    return avFrame;
}

int FilterProcessor::AVFrameToVideoFrame(const AVFrame* avFrame, VideoFrame& frame) {
    // Update frame metadata
    frame.resolution.width = avFrame->width;
    frame.resolution.height = avFrame->height;
    frame.format = avFrame->format;
    frame.pts = avFrame->pts;

    // Copy data pointers
    for (int i = 0; i < 4; i++) {
        frame.data[i] = avFrame->data[i];
        frame.linesize[i] = avFrame->linesize[i];
    }

    return 0;
}

void FilterProcessor::Cleanup() {
    if (videoGraph_) {
        avfilter_graph_free(&videoGraph_);
        videoGraph_ = nullptr;
        videoSrcCtx_ = nullptr;
        videoSinkCtx_ = nullptr;
    }

    if (audioGraph_) {
        avfilter_graph_free(&audioGraph_);
        audioGraph_ = nullptr;
        audioSrcCtx_ = nullptr;
        audioSinkCtx_ = nullptr;
    }
}

// Factory function
std::shared_ptr<IFilterProcessor> CreateFilterProcessor() {
    return FilterProcessor::Create();
}

} // namespace avsdk
