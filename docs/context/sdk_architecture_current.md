# HorsAVSDK 当前架构文档

**状态**: Active  
**创建日期**: 2026-04-21  
**最后更新**: 2026-04-21  
**版本**: v1.0 - 视频播放里程碑

---

## 1. 项目概述

HorsAVSDK 是一个基于 FFmpeg 的跨平台音视频 SDK，当前已实现 **macOS 平台的本地视频文件播放功能**，支持 H.264 解码和 Metal 硬件加速渲染。

### 1.1 当前实现状态

| 功能模块 | 状态 | 说明 |
|---------|------|------|
| 视频解码 (FFmpeg) | ✅ 完成 | 支持 H.264，使用 AVCodecParameters 初始化 |
| 视频渲染 (Metal) | ✅ 完成 | YUV420P → RGB，运行时着色器编译 |
| 音频解码 (FFmpeg) | ✅ 完成 | 支持 AAC，已连接到音频渲染 |
| 音频渲染 (AudioUnit) | ✅ 完成 | AudioUnitRenderer 已集成到播放器 |
| 音画同步 | 🔄 部分 | 音频播放已工作，简单同步已实现 |
| 硬件解码 (VideoToolbox) | 🔄 部分 | 类存在但未在播放器中使用 |
| 网络流媒体 | ❌ 未实现 | NetworkDemuxer 存在但 HTTP 下载未完整 |

---

## 2. 项目结构

```
HorsAVSDK/
├── include/avsdk/           # 公共 C++ 头文件
│   ├── player.h             # IPlayer 主接口
│   ├── demuxer.h            # IDemuxer 解封装接口
│   ├── decoder.h            # IDecoder 解码接口
│   ├── renderer.h           # IRenderer 视频渲染接口
│   ├── audio_renderer.h     # IAudioRenderer 音频渲染接口
│   ├── data_bypass.h        # 数据旁路回调系统
│   └── types.h              # 公共类型定义
├── src/
│   ├── core/                # 核心实现
│   │   ├── demuxer/         # FFmpeg 解封装
│   │   ├── decoder/         # FFmpeg 软解码
│   │   ├── player/          # PlayerImpl 播放器核心
│   │   └── bypass/          # 数据处理旁路
│   ├── platform/            # 平台实现
│   │   └── macos/
│   │       ├── metal_renderer.mm       # Metal 视频渲染
│   │       ├── audio_unit_renderer.mm  # AudioUnit 音频渲染
│   │       ├── videotoolbox_decoder.mm # VideoToolbox 硬解
│   │       └── shaders/                # Metal 着色器
│   └── infrastructure/      # 基础设施
├── examples/                # 示例程序
│   └── macos_player/        # macOS 播放器 Demo
│       ├── PlayerView.mm
│       ├── PlayerWrapper.mm
│       └── ViewController.mm
└── tests/                   # 测试套件
```

---

## 3. 核心接口设计

### 3.1 IPlayer (播放器主接口)

```cpp
class IPlayer {
public:
    virtual ~IPlayer() = default;
    
    // 生命周期
    virtual ErrorCode Initialize(const PlayerConfig& config) = 0;
    virtual ErrorCode Open(const std::string& url) = 0;
    virtual void Close() = 0;
    
    // 播放控制
    virtual ErrorCode Play() = 0;
    virtual ErrorCode Pause() = 0;
    virtual ErrorCode Stop() = 0;
    virtual ErrorCode Seek(Timestamp position_ms) = 0;
    
    // 渲染设置
    virtual void SetRenderView(void* native_window) = 0;
    virtual ErrorCode SetRenderer(std::shared_ptr<IRenderer> renderer) = 0;
    
    // 状态查询
    virtual PlayerState GetState() const = 0;
    virtual MediaInfo GetMediaInfo() const = 0;
    virtual Timestamp GetDuration() const = 0;
};
```

### 3.2 IDemuxer (解封装接口)

```cpp
class IDemuxer {
public:
    virtual ~IDemuxer() = default;
    
    virtual ErrorCode Open(const std::string& url) = 0;
    virtual void Close() = 0;
    virtual AVPacketPtr ReadPacket() = 0;
    virtual ErrorCode Seek(Timestamp position_ms) = 0;
    virtual MediaInfo GetMediaInfo() const = 0;
    
    // 关键：从解封装器获取视频编码参数
    virtual AVCodecParameters* GetVideoCodecParameters() const = 0;
    
    virtual int GetVideoStreamIndex() const = 0;
    virtual int GetAudioStreamIndex() const = 0;
};
```

### 3.3 IDecoder (解码器接口)

```cpp
class IDecoder {
public:
    virtual ~IDecoder() = default;
    
    // 关键：接受 AVCodecParameters 而非硬编码参数
    virtual ErrorCode Initialize(AVCodecParameters* codecpar) = 0;
    virtual AVFramePtr Decode(const AVPacketPtr& packet) = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;
};
```

### 3.4 IRenderer (视频渲染接口)

```cpp
class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    virtual ErrorCode Initialize(void* native_window, int width, int height) = 0;
    virtual ErrorCode RenderFrame(const AVFrame* frame) = 0;
    virtual void Release() = 0;
};
```

---

## 4. 数据流架构

```
┌─────────────────────────────────────────────────────────────┐
│                        PlayerImpl                          │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│  │  FFmpeg     │───▶│  FFmpeg     │───▶│   Metal     │    │
│  │  Demuxer    │    │  Decoder    │    │  Renderer   │    │
│  └─────────────┘    └─────────────┘    └─────────────┘    │
│         │                  │                  │              │
│    ReadPacket()       Decode()          RenderFrame()        │
│         │                  │                  │              │
│    AVPacketPtr        AVFramePtr         Metal Texture      │
│         │                  │                  │              │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐      │
│  │  libavformat│    │  libavcodec│    │   Metal     │      │
│  └─────────────┘    └─────────────┘    └─────────────┘      │
│                                                              │
│  PlaybackLoop (独立线程):                                   │
│  while (playing) {                                           │
│      packet = demuxer->ReadPacket();                         │
│      frame = decoder->Decode(packet);                       │
│      renderer->RenderFrame(frame);                          │
│  }                                                          │
└─────────────────────────────────────────────────────────────┘
```

---

## 5. macOS 平台实现

### 5.1 MetalRenderer 架构

```cpp
class MetalRenderer : public IRenderer {
private:
    void* device_;              // MTLDevice
    void* command_queue_;       // MTLCommandQueue
    void* pipeline_state_;      // MTLRenderPipelineState
    void* view_;                // MTKView
    void* vertex_buffer_;       // MTLBuffer (顶点数据)
    void* index_buffer_;        // MTLBuffer (索引数据)
    void* library_;             // MTLLibrary (着色器)
};
```

### 5.2 渲染流程

1. **Initialize**: 创建设备、命令队列、编译着色器、创建顶点缓冲区
2. **RenderFrame**: 
   - 从 MTKView 获取 CAMetalDrawable
   - 创建 Y/U/V 三个纹理
   - 上传 FFmpeg AVFrame 数据到纹理
   - 执行渲染命令（顶点着色器 + 片段着色器 YUV→RGB 转换）
   - 提交到屏幕

### 5.3 着色器设计

- **Vertex Shader**: 简单的全屏四边形渲染
- **Fragment Shader**: YUV420P → RGB 颜色空间转换

---

## 6. 关键技术决策

### 6.1 为什么选择 AVCodecParameters 初始化解码器？

**问题**: 硬编码 CodecType 会导致 "No start code is found" 错误。

**解决方案**: 从 demuxer 获取 AVCodecParameters，其中包含：
- codec_id: 编码格式（H264/HEVC 等）
- extradata: 编解码器特定的初始化数据（如 SPS/PPS）
- width/height: 视频尺寸
- pix_fmt: 像素格式

**代码示例**:
```cpp
// PlayerImpl::Open()
AVCodecParameters* codecpar = demuxer_->GetVideoCodecParameters();
video_decoder_->Initialize(codecpar);  // 传递完整参数
```

### 6.2 Metal 渲染器设计

**问题**: 如何高效地将 FFmpeg YUV 数据渲染到屏幕？

**解决方案**:
1. 使用三平面纹理（Y, U, V）分别上传
2. 在 GPU 上执行 YUV→RGB 转换（比在 CPU 上快得多）
3. 运行时编译 Metal 着色器（避免预编译依赖）

### 6.3 线程模型

- **主线程**: UI 交互和 SDK API 调用
- **Playback 线程**: 解封装 → 解码 → 渲染（单线程顺序执行）
- **Audio 线程**: AudioUnit 回调驱动（预留，未完全集成）

---

## 7. 已知限制和后续优化

### 7.1 当前限制

1. **硬件解码**: VideoToolboxDecoder 存在但未在播放器中使用
2. **网络流**: NetworkDemuxer 需要完整的 HTTP 下载实现

### 7.2 已解决问题

1. **音频播放**: AudioUnitRenderer 已集成，支持 AAC 解码和播放
   - 实现了 FLTP → S16 音频格式转换
   - 实现了音频预缓冲（0.3-1秒）避免启动卡顿
   - 优化了音频回调性能，最小化锁持有时间
   - 解决了 HALC_ProxyIOContext 过载错误

2. **音频回调优化**: 
   - 原问题：RenderCallback 中执行过多操作（memmove、buffer 整理）导致音频线程过载
   - 解决方案：简化回调逻辑，仅复制指针和大小，实际 memcpy 在锁外执行
   - Buffer 整理操作延迟到每 100 次回调执行一次

### 7.3 性能瓶颈

1. **YUV 数据拷贝**: 每次 RenderFrame 都创建新纹理
   - **优化**: 使用 CVPixelBufferPool 复用纹理内存
2. **单线程播放**: 解封装、解码、渲染串行执行
   - **优化**: 分离线程，使用双缓冲队列
3. **帧率控制**: 使用简单 sleep(33ms)
   - **优化**: 基于音频时钟的精确同步

### 7.4 已知问题

1. **偶发卡顿**: 长时间播放时偶尔出现一次音频卡顿
   - 可能原因：Buffer 整理操作（每 100 次回调）导致偶尔延迟
   - 状态：需要进一步优化，考虑使用无锁环形缓冲区

### 7.5 代码组织

1. **Metal shader**: 内嵌在 C++ 代码中
   - **优化**: 分离到 .metal 文件，预编译为 .metallib
2. **平台判断**: 缺少运行时硬件能力检测
   - **优化**: 添加 VideoToolbox 支持检测

---

## 8. 构建配置

### 8.1 CMake 关键配置

```cmake
# Metal 支持
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)

# macOS App Bundle
set_target_properties(macos_player PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
)
```

### 8.2 依赖库

- FFmpeg 6.1.x: libavcodec, libavformat, libavutil
- Metal/MetalKit: 视频渲染
- AudioUnit: 音频渲染
- Cocoa: macOS UI

---

## 9. 测试覆盖

### 9.1 单元测试

- `decoder_test`: 验证 FFmpegDecoder 初始化和解码
- `demuxer_test`: 验证 FFmpegDemuxer 文件打开和包读取
- `metal_renderer_test`: 验证 Metal 渲染器初始化

### 9.2 集成测试

- `player_integration_test`: 完整播放器流程测试
- macOS Demo: 手动测试视频播放

### 9.3 测试视频

- `integration_test.mp4`: H.264 640x480 5秒测试视频

---

## 10. 参考文档

- [macOS Metal 渲染策略](metal_rendering_strategy.md)
- [SDK 接口设计](av_sdk_interface_design.md)
- [播放器实现详解](player_impl_design.md)
- [FFmpeg 技术方案](ffmpeg_sdk_technical_solution.md)

---

## 11. 更新历史

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-04-21 | v1.0 | 初始文档，视频播放里程碑 |
