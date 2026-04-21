#pragma once
#include "avsdk/renderer.h"

namespace avsdk {

class MetalRenderer : public IRenderer {
public:
    MetalRenderer();
    ~MetalRenderer() override;

    ErrorCode Initialize(void* native_window, int width, int height) override;
    ErrorCode RenderFrame(const AVFrame* frame) override;
    void Release() override;

private:
    void* device_ = nullptr;
    void* command_queue_ = nullptr;
    void* pipeline_state_ = nullptr;
    void* texture_ = nullptr;
    void* view_ = nullptr;
    void* vertex_buffer_ = nullptr;
    void* index_buffer_ = nullptr;
    void* library_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

std::unique_ptr<IRenderer> CreateMetalRenderer();

// Platform renderer factory implementation for macOS/iOS
inline std::unique_ptr<IRenderer> CreatePlatformRenderer() {
    return CreateMetalRenderer();
}

} // namespace avsdk
