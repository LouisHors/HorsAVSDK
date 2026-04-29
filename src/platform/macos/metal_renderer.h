#pragma once
#include "avsdk/renderer.h"
#include <CoreVideo/CoreVideo.h>

namespace avsdk {

class MetalRenderer : public IRenderer {
public:
    MetalRenderer();
    ~MetalRenderer() override;

    ErrorCode Initialize(void* native_window, int width, int height) override;
    ErrorCode RenderFrame(const AVFrame* frame) override;
    void Release() override;

private:
    ErrorCode RenderHardwareFrame(const AVFrame* frame);
    ErrorCode RenderSoftwareFrame(const AVFrame* frame);
    void* device_ = nullptr;
    void* command_queue_ = nullptr;
    void* pipeline_state_ = nullptr;
    void* texture_ = nullptr;
    void* view_ = nullptr;
    void* vertex_buffer_ = nullptr;
    void* index_buffer_ = nullptr;
    void* library_ = nullptr;
    void* metal_layer_ = nullptr;  // Cached CAMetalLayer reference
    void* parent_view_ = nullptr;  // Parent NSView if SDK created MTKView internally
    bool mtk_view_created_by_sdk_ = false;  // Whether we created the MTKView internally
    int width_ = 0;
    int height_ = 0;
};

std::unique_ptr<IRenderer> CreateMetalRenderer();

// Platform renderer factory implementation for macOS/iOS
inline std::unique_ptr<IRenderer> CreatePlatformRenderer() {
    return CreateMetalRenderer();
}

} // namespace avsdk
