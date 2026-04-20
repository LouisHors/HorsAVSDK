#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace avsdk {

// Forward declaration
struct VideoFrame;

// Interface for screenshot handler
class IScreenshotHandler {
public:
    virtual ~IScreenshotHandler() = default;

    // Capture a video frame
    virtual int Capture(const VideoFrame& frame) = 0;

    // Save captured frame to file (PNG/JPG/BMP)
    virtual int Save(const std::string& path) = 0;

    // Get RGB24 data
    virtual int GetData(std::vector<uint8_t>& data, int& width, int& height) = 0;

    // Get PNG encoded data
    virtual int GetPNG(std::vector<uint8_t>& data) = 0;

    // Check if has captured data
    virtual bool HasData() const = 0;

    // Clear captured data
    virtual void Clear() = 0;
};

// Factory function to create screenshot handler
std::shared_ptr<IScreenshotHandler> CreateScreenshotHandler();

} // namespace avsdk
