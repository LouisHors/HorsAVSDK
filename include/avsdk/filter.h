#pragma once
#include <string>
#include <memory>
#include "avsdk/types.h"

namespace avsdk {

// Forward declaration
class FilterProcessor;

// Interface for FFmpeg filter processor
class IFilterProcessor {
public:
    virtual ~IFilterProcessor() = default;

    // Initialize filter graph with filter descriptions
    // videoFilterDesc: FFmpeg filter graph description for video (e.g., "scale=640:480")
    // audioFilterDesc: FFmpeg filter graph description for audio (optional)
    virtual int Initialize(const std::string& videoFilterDesc,
                           const std::string& audioFilterDesc = "") = 0;

    // Process video frame through filter graph (in-place modification)
    virtual int ProcessVideoFrame(VideoFrame& frame) = 0;

    // Process audio frame through filter graph (in-place modification)
    virtual int ProcessAudioFrame(AudioFrame& frame) = 0;

    // Check if filter processor is initialized
    virtual bool IsInitialized() const = 0;

    // Release all resources
    virtual void Release() = 0;
};

// Preset filter strings namespace
namespace Filters {

// Video filters
const std::string Grayscale = "format=gray";
const std::string Sepia = "colorchannelmixer=.393:.769:.189:0:.349:.686:.168:0:.272:.534:.131";
const std::string Invert = "negate";
const std::string HorizontalFlip = "hflip";
const std::string VerticalFlip = "vflip";
const std::string Rotate90 = "transpose=1";
const std::string Rotate180 = "transpose=2,transpose=2";
const std::string Rotate270 = "transpose=2";

// Audio filters
const std::string VolumeUp = "volume=2.0";
const std::string VolumeUpStrong = "volume=3.0";
const std::string VolumeDown = "volume=0.5";
const std::string VolumeDownStrong = "volume=0.3";
const std::string Mono = "pan=mono|c0=0.5*c0+0.5*c1";

// Dynamic filter generators
inline std::string Scale(int width, int height) {
    return "scale=" + std::to_string(width) + ":" + std::to_string(height);
}

inline std::string Crop(int w, int h, int x, int y) {
    return "crop=" + std::to_string(w) + ":" + std::to_string(h) +
           ":" + std::to_string(x) + ":" + std::to_string(y);
}

inline std::string Adjust(float brightness, float contrast, float saturation) {
    return "eq=brightness=" + std::to_string(brightness) +
           ":contrast=" + std::to_string(contrast) +
           ":saturation=" + std::to_string(saturation);
}

inline std::string Volume(float level) {
    return "volume=" + std::to_string(level);
}

} // namespace Filters

// Factory function
std::shared_ptr<IFilterProcessor> CreateFilterProcessor();

} // namespace avsdk
