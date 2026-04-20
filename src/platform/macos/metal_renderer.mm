#import "metal_renderer.h"
#import "avsdk/logger.h"
#import <Metal/Metal.h>
#import <Cocoa/Cocoa.h>
extern "C" {
#include <libavutil/frame.h>
}

namespace avsdk {

MetalRenderer::MetalRenderer() = default;

MetalRenderer::~MetalRenderer() {
    Release();
}

ErrorCode MetalRenderer::Initialize(void* native_window, int width, int height) {
    width_ = width;
    height_ = height;

    device_ = (__bridge_retained void*)MTLCreateSystemDefaultDevice();
    if (!device_) {
        LOG_ERROR("MetalRenderer", "Failed to create Metal device");
        return ErrorCode::HardwareNotAvailable;
    }

    id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
    id<MTLCommandQueue> queue = [device newCommandQueue];
    command_queue_ = (__bridge_retained void*)queue;

    LOG_INFO("MetalRenderer", "Initialized " + std::to_string(width) + "x" + std::to_string(height));
    return ErrorCode::OK;
}

ErrorCode MetalRenderer::RenderFrame(const AVFrame* frame) {
    if (!frame || !device_ || !command_queue_) {
        return ErrorCode::InvalidParameter;
    }

    // TODO: Implement actual Metal rendering
    // For now, just log
    LOG_INFO("MetalRenderer", "Rendering frame " + std::to_string(frame->width) + "x" + std::to_string(frame->height));

    return ErrorCode::OK;
}

void MetalRenderer::Release() {
    if (texture_) {
        CFRelease(texture_);
        texture_ = nullptr;
    }
    if (pipeline_state_) {
        CFRelease(pipeline_state_);
        pipeline_state_ = nullptr;
    }
    if (command_queue_) {
        CFRelease(command_queue_);
        command_queue_ = nullptr;
    }
    if (device_) {
        CFRelease(device_);
        device_ = nullptr;
    }
}

std::unique_ptr<IRenderer> CreateMetalRenderer() {
    return std::make_unique<MetalRenderer>();
}

} // namespace avsdk
