#pragma once
#include "avsdk/filter.h"
#include "avsdk/data_bypass.h"
#include "avsdk/types.h"
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>

// Forward declarations for FFmpeg filter types
struct AVFilterGraph;
struct AVFilterContext;
struct AVFilterInOut;
struct AVFrame;

namespace avsdk {

// FilterProcessor implementation using FFmpeg filtergraph
class FilterProcessor : public IFilterProcessor, public IDataBypass {
public:
    FilterProcessor();
    ~FilterProcessor() override;

    // IFilterProcessor interface
    int Initialize(const std::string& videoFilterDesc,
                   const std::string& audioFilterDesc = "") override;
    int ProcessVideoFrame(VideoFrame& frame) override;
    int ProcessAudioFrame(AudioFrame& frame) override;
    bool IsInitialized() const override;
    void Release() override;

    // IDataBypass interface - calls Process methods on pre-render frames
    void OnRawPacket(const EncodedPacket& packet) override;
    void OnDecodedVideoFrame(const VideoFrame& frame) override;
    void OnDecodedAudioFrame(const AudioFrame& frame) override;
    void OnPreRenderVideoFrame(VideoFrame& frame) override;
    void OnPreRenderAudioFrame(AudioFrame& frame) override;
    void OnEncodedPacket(const EncodedPacket& packet) override;

    // Static Create method
    static std::shared_ptr<FilterProcessor> Create();

private:
    mutable std::mutex mutex_;

    // Filter descriptions (stored for lazy initialization)
    std::string videoFilterDesc_;
    std::string audioFilterDesc_;

    // Video filter graph
    AVFilterGraph* videoGraph_;
    AVFilterContext* videoSrcCtx_;
    AVFilterContext* videoSinkCtx_;

    // Audio filter graph
    AVFilterGraph* audioGraph_;
    AVFilterContext* audioSrcCtx_;
    AVFilterContext* audioSinkCtx_;

    // Format tracking for lazy initialization
    bool videoInitialized_;
    bool audioInitialized_;
    int videoWidth_;
    int videoHeight_;
    int videoFormat_;
    int audioSampleRate_;
    int audioChannels_;
    uint64_t audioChannelLayout_;

    // Initialize video filter graph with known format
    int InitVideoFilterGraph(int width, int height, int format);

    // Initialize audio filter graph with known format
    int InitAudioFilterGraph(int sampleRate, int channels, uint64_t channelLayout);

    // Convert VideoFrame to AVFrame
    AVFrame* VideoFrameToAVFrame(const VideoFrame& frame);

    // Convert AVFrame to VideoFrame
    int AVFrameToVideoFrame(const AVFrame* avFrame, VideoFrame& frame);

    // Cleanup all resources
    void Cleanup();
};

} // namespace avsdk
