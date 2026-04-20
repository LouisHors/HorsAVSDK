#pragma once
#include "avsdk/screenshot.h"
#include "avsdk/data_bypass.h"
#include "avsdk/types.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace avsdk {

// ScreenshotHandler implementation
class ScreenshotHandler : public IScreenshotHandler, public IDataBypass {
public:
    ScreenshotHandler();
    ~ScreenshotHandler() override;

    // IScreenshotHandler interface
    int Capture(const VideoFrame& frame) override;
    int Save(const std::string& path) override;
    int GetData(std::vector<uint8_t>& data, int& width, int& height) override;
    int GetPNG(std::vector<uint8_t>& data) override;
    bool HasData() const override;
    void Clear() override;

    // IDataBypass interface - calls Capture()
    void OnRawPacket(const EncodedPacket& packet) override;
    void OnDecodedVideoFrame(const VideoFrame& frame) override;
    void OnDecodedAudioFrame(const AudioFrame& frame) override;
    void OnPreRenderVideoFrame(VideoFrame& frame) override;
    void OnPreRenderAudioFrame(AudioFrame& frame) override;
    void OnEncodedPacket(const EncodedPacket& packet) override;

    // Static Create method
    static std::shared_ptr<ScreenshotHandler> Create();

private:
    mutable std::mutex mutex_;
    std::vector<uint8_t> rgbData_;
    int width_;
    int height_;
    bool hasData_;

    // Convert VideoFrame to RGB24 using sws_scale
    int ConvertToRGB(const VideoFrame& frame, std::vector<uint8_t>& output, int& outWidth, int& outHeight);
};

} // namespace avsdk
