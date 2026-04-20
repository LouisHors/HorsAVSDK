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
    int width_ = 0;
    int height_ = 0;
};

std::unique_ptr<IRenderer> CreateMetalRenderer();

} // namespace avsdk
