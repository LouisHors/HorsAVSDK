# macOS Metal 视频渲染策略

**状态**: Active  
**创建日期**: 2026-04-21  
**最后更新**: 2026-04-21  
**适用范围**: macOS 10.14+, iOS 12+

---

## 1. 设计目标

实现一个高效、低延迟的视频渲染系统，将 FFmpeg 解码后的 YUV420P 数据转换为屏幕上的 RGB 图像。

### 1.1 关键指标

- **延迟**: < 16ms (60fps 单帧预算)
- **CPU 占用**: 解码和渲染 < 30% (1080p@30fps on M1 Mac)
- **内存**: 避免每帧内存分配
- **格式支持**: YUV420P (主要), NV12 (预留)

---

## 2. 架构设计

### 2.1 系统架构图

```
┌────────────────────────────────────────────────────────────┐
│                    MetalRenderer                           │
├────────────────────────────────────────────────────────────┤
│                                                            │
│   ┌─────────────┐     ┌─────────────┐     ┌─────────────┐ │
│   │   FFmpeg    │     │    Metal    │     │   CAMetal   │ │
│   │   AVFrame   │────▶│   Textures  │────▶│   Drawable  │ │
│   │   (YUV420P) │     │  (Y/U/V)    │     │   (BGRA8)   │ │
│   └─────────────┘     └─────────────┘     └─────────────┘ │
│          │                   │                   │          │
│          ▼                   ▼                   ▼          │
│   ┌─────────────────────────────────────────────────────┐  │
│   │              Render Command Encoder                  │  │
│   │                                                      │  │
│   │   Vertex Shader          Fragment Shader             │  │
│   │   ┌─────────────┐        ┌─────────────────────┐    │  │
│   │   │  Position   │        │  YUV → RGB          │    │  │
│   │   │  TexCoord   │───────▶│  Sampler + Blend    │    │  │
│   │   └─────────────┘        └─────────────────────┘    │  │
│   │                                                      │  │
│   └─────────────────────────────────────────────────────┘  │
│                            │                                │
│                            ▼                                │
│                    ┌─────────────┐                          │
│                    │   Screen    │                          │
│                    └─────────────┘                          │
└────────────────────────────────────────────────────────────┘
```

### 2.2 类结构

```cpp
namespace avsdk {

class MetalRenderer : public IRenderer {
public:
    ErrorCode Initialize(void* native_window, int width, int height);
    ErrorCode RenderFrame(const AVFrame* frame);
    void Release();

private:
    // Metal 对象
    void* device_;              // id<MTLDevice>
    void* command_queue_;       // id<MTLCommandQueue>
    void* pipeline_state_;      // id<MTLRenderPipelineState>
    void* library_;             // id<MTLLibrary>
    
    // 渲染目标
    void* view_;                // MTKView
    
    // 几何数据
    void* vertex_buffer_;       // id<MTLBuffer> (位置+纹理坐标)
    void* index_buffer_;        // id<MTLBuffer> (三角形索引)
    
    // 配置
    int width_ = 0;
    int height_ = 0;
};

}
```

---

## 3. 关键技术决策

### 3.1 为什么使用三平面纹理而非 CVPixelBuffer?

| 方案 | 优点 | 缺点 | 当前选择 |
|------|------|------|----------|
| **三平面 MTLTexture** | 直接上传 FFmpeg 数据，通用 | 需要格式转换 | ✅ 当前 |
| **CVPixelBuffer** | 零拷贝，硬件解码友好 | 仅 VideoToolbox 可用 | 🔄 未来优化 |
| **单平面 NV12** | U/V 交错，减少纹理数 | FFmpeg 默认 YUV420P | 🔄 预留支持 |

**结论**: 当前 FFmpeg 软解码输出 YUV420P，三平面纹理是最直接的方案。

### 3.2 为什么在 GPU 上执行 YUV→RGB 转换？

**CPU 转换方案**:
```cpp
// 慢：需要遍历所有像素
for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
        // YUV → RGB 计算
    }
}
// 然后上传到 GPU
```

**GPU 转换方案** (当前实现):
```metal
// 每个像素一个 GPU 线程，并行执行
fragment float4 fragmentShaderYUV(...) {
    float y = yTexture.sample(sampler, texCoord).r;
    float u = uTexture.sample(sampler, texCoord).r - 0.5;
    float v = vTexture.sample(sampler, texCoord).r - 0.5;
    // YUV → RGB 计算
    return float4(r, g, b, 1.0);
}
```

**性能对比** (1080p 帧):
- CPU: ~5-10ms
- GPU: ~0.1-0.2ms (60倍加速)

### 3.3 为什么运行时编译着色器？

**预编译方案**:
- 需要 .metal → .air → .metallib 编译步骤
- CMake 集成复杂
- 跨 Xcode 版本兼容性问题

**运行时编译方案** (当前):
```cpp
NSString* shaderSource = @"...";  // 内嵌源码
id<MTLLibrary> library = [device newLibraryWithSource:shaderSource 
                                               options:nil 
                                                 error:&error];
```

**优点**:
- 构建简单，无需额外编译步骤
- 源码和 C++ 在一起，易于维护

**缺点**:
- 首次渲染有编译延迟 (~50-100ms)
- 运行时错误难以调试

**优化方向**: 预编译为 .metallib 并嵌入到资源包中。

---

## 4. 渲染流程详解

### 4.1 Initialize 流程

```cpp
ErrorCode MetalRenderer::Initialize(void* native_window, int width, int height) {
    // 1. 创建 Metal 设备
    device_ = MTLCreateSystemDefaultDevice();
    
    // 2. 创建命令队列
    command_queue_ = [device newCommandQueue];
    
    // 3. 保存 MTKView 引用
    view_ = (MTKView*)native_window;
    view_.device = device;
    
    // 4. 编译着色器
    library_ = [device newLibraryWithSource:shaderSource options:nil error:&error];
    
    // 5. 创建渲染管线
    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = [library newFunctionWithName:@"vertexShader"];
    desc.fragmentFunction = [library newFunctionWithName:@"fragmentShaderYUV"];
    desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    desc.vertexDescriptor = vertexDesc;  // 顶点布局
    pipeline_state_ = [device newRenderPipelineStateWithDescriptor:desc error:&error];
    
    // 6. 创建顶点缓冲区（全屏四边形）
    vertex_buffer_ = [device newBufferWithBytes:kVertexData length:sizeof(kVertexData)];
    index_buffer_ = [device newBufferWithBytes:kIndexData length:sizeof(kIndexData)];
}
```

### 4.2 RenderFrame 流程

```cpp
ErrorCode MetalRenderer::RenderFrame(const AVFrame* frame) {
    // 1. 获取可绘制表面
    CAMetalLayer* metalLayer = (CAMetalLayer*)view.layer;
    id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
    
    // 2. 创建 Y/U/V 纹理并上传数据
    id<MTLTexture> yTexture = createTexture(frame->data[0], frame->linesize[0], 
                                             width, height);
    id<MTLTexture> uTexture = createTexture(frame->data[1], frame->linesize[1], 
                                             width/2, height/2);
    id<MTLTexture> vTexture = createTexture(frame->data[2], frame->linesize[2], 
                                             width/2, height/2);
    
    // 3. 创建命令缓冲区
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    
    // 4. 配置渲染通道
    MTLRenderPassDescriptor* passDesc = [[MTLRenderPassDescriptor alloc] init];
    passDesc.colorAttachments[0].texture = drawable.texture;
    passDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDesc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
    
    // 5. 创建渲染编码器
    id<MTLRenderCommandEncoder> encoder = 
        [commandBuffer renderCommandEncoderWithDescriptor:passDesc];
    
    // 6. 设置渲染状态
    [encoder setRenderPipelineState:pipeline];
    [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    [encoder setFragmentTexture:yTexture atIndex:0];
    [encoder setFragmentTexture:uTexture atIndex:1];
    [encoder setFragmentTexture:vTexture atIndex:2];
    
    // 7. 绘制
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:6
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:indexBuffer
                 indexBufferOffset:0];
    
    // 8. 提交
    [encoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}
```

### 4.3 顶点布局

```
顶点数据结构（每个顶点 16 字节）：
┌──────────────┬──────────────┐
│   Position   │   TexCoord   │
│   (float2)   │   (float2)   │
├──────────────┼──────────────┤
│   -1, 1      │    0, 0      │  // 左上
│   -1, -1     │    0, 1      │  // 左下
│   1, -1      │    1, 1      │  // 右下
│   1, 1       │    1, 0      │  // 右上
└──────────────┴──────────────┘

索引数据（两个三角形）：
┌─────────┐
│ 0, 1, 2 │  // 第一个三角形 (左上, 左下, 右下)
│ 0, 2, 3 │  // 第二个三角形 (左上, 右下, 右上)
└─────────┘
```

---

## 5. 着色器实现

### 5.1 顶点着色器

```metal
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 position [[attribute(0)]];   // 裁剪空间位置
    float2 texCoord [[attribute(1)]];   // 纹理坐标
};

struct VertexOut {
    float4 position [[position]];       // 裁剪空间位置
    float2 texCoord;                     // 纹理坐标
};

vertex VertexOut vertexShader(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.position = float4(in.position, 0.0, 1.0);  // z=0, w=1
    out.texCoord = in.texCoord;
    return out;
}
```

### 5.2 片段着色器（YUV→RGB）

```metal
fragment float4 fragmentShaderYUV(VertexOut in [[stage_in]],
                                   texture2d<float> yTexture [[texture(0)]],
                                   texture2d<float> uTexture [[texture(1)]],
                                   texture2d<float> vTexture [[texture(2)]]) {
    constexpr sampler textureSampler(
        mag_filter::linear,     // 放大过滤
        min_filter::linear      // 缩小过滤
    );
    
    // 采样 YUV
    float y = yTexture.sample(textureSampler, in.texCoord).r;
    float u = uTexture.sample(textureSampler, in.texCoord).r - 0.5;
    float v = vTexture.sample(textureSampler, in.texCoord).r - 0.5;
    
    // BT.601 YUV → RGB 转换
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;
    
    return float4(r, g, b, 1.0);
}
```

### 5.3 颜色空间说明

- **输入**: YUV420P (BT.601 标准)
  - Y: 亮度 (0-1)
  - U/V: 色度 (-0.5 到 0.5)
- **输出**: BGRA8 (Metal 默认帧缓冲区格式)
  - BGRA: 蓝-绿-红-透明度
  - 8-bit 每通道

---

## 6. 性能优化策略

### 6.1 当前优化

| 优化点 | 实现 | 效果 |
|--------|------|------|
| GPU YUV→RGB | 片段着色器并行转换 | 60x 加速 vs CPU |
| 顶点缓冲复用 | 初始化时创建，渲染时复用 | 避免每帧分配 |
| 线性采样 | mag/min_filter::linear | 平滑缩放 |
| nextDrawable | CAMetalLayer 直接获取 | 避免 MTKView 自动渲染冲突 |

### 6.2 未来优化

| 优化点 | 方案 | 预期效果 |
|--------|------|----------|
| 纹理池 | CVPixelBufferPool/MTLTexturePool | 减少内存分配 |
| 零拷贝 | IOSurface/CVPixelBuffer | 避免 CPU→GPU 拷贝 |
| 预编译着色器 | .metallib 嵌入资源 | 减少首次渲染延迟 |
| 多线程渲染 | 双缓冲 | 解码和渲染并行 |
| MTLBlitCommandEncoder | 快速纹理上传 | 优化数据拷贝 |

---

## 7. 错误处理

### 7.1 常见错误

| 错误 | 原因 | 解决方案 |
|------|------|----------|
| `Failed to create pipeline state: Vertex function has input attributes but no vertex descriptor was set` | 顶点着色器需要顶点描述符但未设置 | 创建 MTLVertexDescriptor 并设置到 pipeline descriptor |
| `[CAMetalLayerDrawable texture] should not be called after already presenting this drawable` | 重复使用了已提交的 drawable | 使用 `nextDrawable` 获取新 drawable |
| `Failed to create shader library` | 着色器源码语法错误 | 检查 Metal 语法兼容性 |

### 7.2 调试技巧

```cpp
// 1. 捕获 GPU 帧
Xcode → Debug → Capture GPU Frame

// 2. 验证纹理上传
LOG_INFO("Metal", "Y texture: " + std::to_string(yWidth) + "x" + std::to_string(yHeight));

// 3. 检查像素格式
LOG_INFO("Metal", "Frame format: " + std::to_string(frame->format));
// AV_PIX_FMT_YUV420P = 0
```

---

## 8. 平台差异

### 8.1 macOS vs iOS

| 特性 | macOS | iOS | 注意事项 |
|------|-------|-----|----------|
| MTKView | ✅ 支持 | ✅ 支持 | 相同 API |
| CAMetalLayer | ✅ 支持 | ✅ 支持 | 相同 API |
| 内存管理 | 更宽松 | 更严格 | iOS 需要更谨慎的内存管理 |
| 屏幕分辨率 | 可变 | 固定 | macOS 需要处理窗口缩放 |

### 8.2 Metal 版本

- **最低要求**: Metal 1.0 (macOS 10.11, iOS 8)
- **当前使用**: Metal 2.0+ 特性（运行时编译）
- **建议目标**: Metal 3.0 (macOS 13, iOS 16) 用于未来功能

---

## 9. 参考

- [Metal Programming Guide](https://developer.apple.com/library/archive/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/)
- [Metal Shading Language Specification](https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf)
- [FFmpeg Pix_FMT 文档](https://ffmpeg.org/doxygen/trunk/pixfmt_8h.html)
- [BT.601 Color Space](https://en.wikipedia.org/wiki/Rec._601)
