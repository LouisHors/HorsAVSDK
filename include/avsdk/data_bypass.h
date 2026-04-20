#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <algorithm>
#include "avsdk/types.h"

namespace avsdk {

// Data structures are now from types.h
// - VideoResolution
// - EncodedPacket (needs to be added to types.h)
// - VideoFrame (needs to be added to types.h)
// - AudioFrame (needs to be added to types.h)

// Interface for data bypass callbacks
class IDataBypass {
public:
    virtual ~IDataBypass() = default;

    // Called after demux, before decode
    virtual void OnRawPacket(const EncodedPacket& packet) = 0;

    // Called after video decode
    virtual void OnDecodedVideoFrame(const VideoFrame& frame) = 0;

    // Called after audio decode
    virtual void OnDecodedAudioFrame(const AudioFrame& frame) = 0;

    // Called before video render (allows modification)
    virtual void OnPreRenderVideoFrame(VideoFrame& frame) = 0;

    // Called before audio render (allows modification)
    virtual void OnPreRenderAudioFrame(AudioFrame& frame) = 0;

    // Called after encode
    virtual void OnEncodedPacket(const EncodedPacket& packet) = 0;
};

// Struct with std::function callbacks
struct DataBypassCallbacks {
    std::function<void(const EncodedPacket&)> onRawPacket;
    std::function<void(const VideoFrame&)> onDecodedVideoFrame;
    std::function<void(const AudioFrame&)> onDecodedAudioFrame;
    std::function<void(VideoFrame&)> onPreRenderVideoFrame;
    std::function<void(AudioFrame&)> onPreRenderAudioFrame;
    std::function<void(const EncodedPacket&)> onEncodedPacket;
};

// Manager class for data bypass handlers
class DataBypassManager {
public:
    DataBypassManager() = default;
    ~DataBypassManager() = default;

    // Register a bypass handler
    void RegisterBypass(std::shared_ptr<IDataBypass> bypass);

    // Unregister a bypass handler
    void UnregisterBypass(std::shared_ptr<IDataBypass> bypass);

    // Clear all handlers
    void ClearBypasses();

    // Dispatch methods - called from various pipeline stages
    void DispatchRawPacket(const EncodedPacket& packet);
    void DispatchDecodedVideoFrame(const VideoFrame& frame);
    void DispatchDecodedAudioFrame(const AudioFrame& frame);
    void DispatchPreRenderVideoFrame(VideoFrame& frame);
    void DispatchPreRenderAudioFrame(AudioFrame& frame);
    void DispatchEncodedPacket(const EncodedPacket& packet);

    // Get handler count
    size_t GetBypassCount() const;

private:
    mutable std::mutex mutex_;
    std::vector<std::weak_ptr<IDataBypass>> bypasses_;

    // Helper to clean expired weak_ptr and get valid handlers
    std::vector<std::shared_ptr<IDataBypass>> GetValidBypasses();
};

} // namespace avsdk
