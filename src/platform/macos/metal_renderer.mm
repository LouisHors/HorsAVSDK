#import "metal_renderer.h"
#import "avsdk/logger.h"
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

namespace avsdk {

// Vertex data for a full-screen quad (position + texcoord)
static const float kVertexData[] = {
    // Positions      // Texture coords
    -1.0f,  1.0f,    0.0f, 0.0f,  // Top-left
    -1.0f, -1.0f,    0.0f, 1.0f,  // Bottom-left
     1.0f, -1.0f,    1.0f, 1.0f,  // Bottom-right
     1.0f,  1.0f,    1.0f, 0.0f,  // Top-right
};

static const uint16_t kIndexData[] = {
    0, 1, 2,  // First triangle
    0, 2, 3,  // Second triangle
};

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

    // Create MTKView from native window
    if (native_window) {
        view_ = (__bridge_retained void*)(__bridge MTKView*)native_window;
        MTKView* view = (__bridge MTKView*)view_;
        view.device = device;
        view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        view.depthStencilPixelFormat = MTLPixelFormatInvalid;
    }

    // Load shaders from default library
    NSError* error = nil;
    id<MTLLibrary> library = [device newDefaultLibraryWithBundle:[NSBundle mainBundle] error:&error];
    if (!library) {
        // Try to load from source if not compiled
        NSString* shaderSource = @R"(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
};

vertex VertexOut vertexShader(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.position = float4(in.position, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

fragment float4 fragmentShaderYUV(VertexOut in [[stage_in]],
                                   texture2d<float> yTexture [[texture(0)]],
                                   texture2d<float> uTexture [[texture(1)]],
                                   texture2d<float> vTexture [[texture(2)]]) {
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);
    float y = yTexture.sample(textureSampler, in.texCoord).r;
    float u = uTexture.sample(textureSampler, in.texCoord).r - 0.5;
    float v = vTexture.sample(textureSampler, in.texCoord).r - 0.5;
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;
    return float4(r, g, b, 1.0);
}
)";
        library = [device newLibraryWithSource:shaderSource options:nil error:&error];
        if (!library) {
            LOG_ERROR("MetalRenderer", "Failed to create shader library: " + std::string([error.localizedDescription UTF8String]));
            return ErrorCode::Unknown;
        }
    }
    library_ = (__bridge_retained void*)library;

    // Create pipeline state
    id<MTLFunction> vertexFunc = [library newFunctionWithName:@"vertexShader"];
    id<MTLFunction> fragmentFunc = [library newFunctionWithName:@"fragmentShaderYUV"];

    if (!vertexFunc || !fragmentFunc) {
        LOG_ERROR("MetalRenderer", "Failed to create shader functions");
        return ErrorCode::Unknown;
    }

    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = vertexFunc;
    desc.fragmentFunction = fragmentFunc;
    desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Create vertex descriptor BEFORE creating pipeline
    MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
    vertexDesc.attributes[0].format = MTLVertexFormatFloat2;
    vertexDesc.attributes[0].offset = 0;
    vertexDesc.attributes[0].bufferIndex = 0;
    vertexDesc.attributes[1].format = MTLVertexFormatFloat2;
    vertexDesc.attributes[1].offset = sizeof(float) * 2;
    vertexDesc.attributes[1].bufferIndex = 0;
    vertexDesc.layouts[0].stride = sizeof(float) * 4;
    desc.vertexDescriptor = vertexDesc;

    id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (!pipeline) {
        LOG_ERROR("MetalRenderer", "Failed to create pipeline state: " + std::string([error.localizedDescription UTF8String]));
        return ErrorCode::Unknown;
    }
    pipeline_state_ = (__bridge_retained void*)pipeline;

    // Create vertex buffer
    id<MTLBuffer> vertexBuffer = [device newBufferWithBytes:kVertexData
                                                        length:sizeof(kVertexData)
                                                       options:MTLResourceStorageModeShared];
    vertex_buffer_ = (__bridge_retained void*)vertexBuffer;

    // Create index buffer
    id<MTLBuffer> indexBuffer = [device newBufferWithBytes:kIndexData
                                                       length:sizeof(kIndexData)
                                                      options:MTLResourceStorageModeShared];
    index_buffer_ = (__bridge_retained void*)indexBuffer;

    LOG_INFO("MetalRenderer", "Initialized " + std::to_string(width) + "x" + std::to_string(height));
    return ErrorCode::OK;
}

ErrorCode MetalRenderer::RenderFrame(const AVFrame* frame) {
    if (!frame || !device_ || !command_queue_ || !pipeline_state_) {
        return ErrorCode::InvalidParameter;
    }

    id<MTLDevice> device = (__bridge id<MTLDevice>)device_;
    id<MTLCommandQueue> commandQueue = (__bridge id<MTLCommandQueue>)command_queue_;
    MTKView* view = (__bridge MTKView*)view_;

    if (!view) {
        return ErrorCode::InvalidParameter;
    }

    // Get drawable from CAMetalLayer
    CAMetalLayer* metalLayer = (CAMetalLayer*)view.layer;
    id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
    if (!drawable) {
        // No drawable available, skip this frame
        return ErrorCode::OK;
    }

    // Create command buffer
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];

    // Create render pass descriptor
    MTLRenderPassDescriptor* passDesc = [[MTLRenderPassDescriptor alloc] init];
    passDesc.colorAttachments[0].texture = drawable.texture;
    passDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    passDesc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

    // Create render encoder
    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:passDesc];

    // Set pipeline state
    id<MTLRenderPipelineState> pipeline = (__bridge id<MTLRenderPipelineState>)pipeline_state_;
    [encoder setRenderPipelineState:pipeline];

    // Set vertex buffer
    id<MTLBuffer> vertexBuffer = (__bridge id<MTLBuffer>)vertex_buffer_;
    [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];

    // Upload YUV textures
    if (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P) {
        // Upload Y plane
        int yWidth = frame->width;
        int yHeight = frame->height;
        int uvWidth = yWidth / 2;
        int uvHeight = yHeight / 2;

        // Create Y texture
        MTLTextureDescriptor* yDesc = [[MTLTextureDescriptor alloc] init];
        yDesc.pixelFormat = MTLPixelFormatR8Unorm;
        yDesc.width = yWidth;
        yDesc.height = yHeight;
        yDesc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> yTexture = [device newTextureWithDescriptor:yDesc];

        // Create U texture
        MTLTextureDescriptor* uDesc = [[MTLTextureDescriptor alloc] init];
        uDesc.pixelFormat = MTLPixelFormatR8Unorm;
        uDesc.width = uvWidth;
        uDesc.height = uvHeight;
        uDesc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> uTexture = [device newTextureWithDescriptor:uDesc];

        // Create V texture
        MTLTextureDescriptor* vDesc = [[MTLTextureDescriptor alloc] init];
        vDesc.pixelFormat = MTLPixelFormatR8Unorm;
        vDesc.width = uvWidth;
        vDesc.height = uvHeight;
        vDesc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> vTexture = [device newTextureWithDescriptor:vDesc];

        // Upload data
        [yTexture replaceRegion:MTLRegionMake2D(0, 0, yWidth, yHeight)
                    mipmapLevel:0
                      withBytes:frame->data[0]
                    bytesPerRow:frame->linesize[0]];
        [uTexture replaceRegion:MTLRegionMake2D(0, 0, uvWidth, uvHeight)
                    mipmapLevel:0
                      withBytes:frame->data[1]
                    bytesPerRow:frame->linesize[1]];
        [vTexture replaceRegion:MTLRegionMake2D(0, 0, uvWidth, uvHeight)
                    mipmapLevel:0
                      withBytes:frame->data[2]
                    bytesPerRow:frame->linesize[2]];

        // Set fragment textures
        [encoder setFragmentTexture:yTexture atIndex:0];
        [encoder setFragmentTexture:uTexture atIndex:1];
        [encoder setFragmentTexture:vTexture atIndex:2];
    }

    // Draw
    id<MTLBuffer> indexBuffer = (__bridge id<MTLBuffer>)index_buffer_;
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:6
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:indexBuffer
                 indexBufferOffset:0];

    // End encoding
    [encoder endEncoding];

    // Present drawable
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];

    return ErrorCode::OK;
}

void MetalRenderer::Release() {
    if (view_) {
        CFRelease(view_);
        view_ = nullptr;
    }
    if (vertex_buffer_) {
        CFRelease(vertex_buffer_);
        vertex_buffer_ = nullptr;
    }
    if (index_buffer_) {
        CFRelease(index_buffer_);
        index_buffer_ = nullptr;
    }
    if (library_) {
        CFRelease(library_);
        library_ = nullptr;
    }
    if (pipeline_state_) {
        CFRelease(pipeline_state_);
        pipeline_state_ = nullptr;
    }
    if (texture_) {
        CFRelease(texture_);
        texture_ = nullptr;
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
