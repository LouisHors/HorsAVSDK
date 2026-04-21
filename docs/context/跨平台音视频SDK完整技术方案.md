# 跨平台音视频SDK完整技术方案

## 文档信息

| 项目 | 内容 |
|------|------|
| 文档版本 | v1.0 |
| 创建日期 | 2024年 |
| 目标平台 | iOS/iPadOS、Android、macOS、Windows |
| 技术栈 | FFmpeg 6.1 + C++11/14 + 平台原生框架 |

---

## 目录

1. [项目概述](#1-项目概述)
2. [需求文档（PRD）](#2-需求文档prd)
3. [架构设计文档](#3-架构设计文档)
4. [接口设计文档](#4-接口设计文档)
5. [FFmpeg技术实现方案](#5-ffmpeg技术实现方案)
6. [开发路线图](#6-开发路线图)
7. [总结](#7-总结)

---

## 1. 项目概述

### 1.1 项目目标

开发一个基于FFmpeg的跨平台音视频编解码渲染播放SDK，支持iOS（含iPadOS）、Android、macOS、Windows四个平台。

### 1.2 核心要求

1. **SDK底层由C++实现**，用于支持多平台
2. **先实现macOS版本**作为验证平台（C++ → Objective-C转换容易）
3. **支持H265编码**，以及视频软硬编解码
4. **分四个阶段实现**：
   - 第一阶段：本地音视频文件播放
   - 第二阶段：在线音视频及RTMP、FLV、HLS等串流内容播放
   - 第三阶段：实时编码音视频
   - 第四阶段：音视频所有管道流程的数据旁路回调

### 1.3 技术架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│                        应用层 (Application)                        │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐  │
│  │ iOS/macOS   │  │   Android   │  │   Windows   │  │  其他   │  │
│  │  Obj-C/Swift│  │  Java/Kotlin│  │   C++/C#    │  │  平台   │  │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └────┬────┘  │
├─────────┼────────────────┼────────────────┼──────────────┼───────┤
│         └────────────────┴────────────────┴──────────────┘       │
│                      平台适配层 (Platform Adapter)                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐  │
│  │VideoToolbox │  │ MediaCodec  │  │  DXVA/D3D   │  │  ...    │  │
│  │   /Metal    │  │  /OpenGL ES │  │   DirectX   │  │         │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                         C++ 核心层                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐  │
│  │   Player    │  │   Encoder   │  │   Decoder   │  │  Utils  │  │
│  │   Core      │  │    Core     │  │    Core     │  │  Core   │  │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └────┬────┘  │
│         └────────────────┴────────────────┴──────────────┘       │
├─────────────────────────────────────────────────────────────────┤
│                         FFmpeg 层                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐  │
│  │  libavcodec │  │ libavformat │  │  libswscale │  │  ...    │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. 需求文档（PRD）

### 2.1 功能需求

#### 2.1.1 播放功能需求

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| PLAY-001 | 支持常见视频格式：MP4、MOV、MKV、AVI、FLV、WMV | P0 |
| PLAY-002 | 支持常见音频格式：AAC、MP3、AC3、FLAC、WAV | P0 |
| PLAY-003 | 支持H264/H265视频解码（软硬解） | P0 |
| PLAY-004 | 支持多音轨切换 | P1 |
| PLAY-005 | 支持字幕显示（内嵌/外挂） | P2 |
| PLAY-006 | 支持播放速度调节（0.5x-2.0x） | P1 |
| PLAY-007 | 支持循环播放模式 | P1 |

#### 2.1.2 网络流播放需求

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| STREAM-001 | 支持HTTP/HTTPS协议播放 | P0 |
| STREAM-002 | 支持RTMP协议播放 | P0 |
| STREAM-003 | 支持HLS协议播放（m3u8） | P0 |
| STREAM-004 | 支持FLV over HTTP播放 | P0 |
| STREAM-005 | 支持DASH协议播放 | P1 |
| STREAM-007 | 支持网络自适应缓冲 | P0 |
| STREAM-008 | 支持断线重连机制 | P0 |

#### 2.1.3 编解码功能需求

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| VDEC-001 | 支持H264软件解码 | P0 |
| VDEC-002 | 支持H265/HEVC软件解码 | P0 |
| VDEC-003 | 支持H264硬件解码 | P0 |
| VDEC-004 | 支持H265硬件解码 | P0 |
| VENC-001 | 支持H264软件编码 | P0 |
| VENC-002 | 支持H265软件编码 | P0 |
| VENC-003 | 支持H264硬件编码 | P0 |
| VENC-004 | 支持H265硬件编码 | P0 |

#### 2.1.4 数据旁路需求（第四阶段）

| 需求ID | 需求描述 | 优先级 |
|--------|----------|--------|
| BYPASS-001 | 原始音视频数据回调（解封装后） | P0 |
| BYPASS-002 | 解码后视频帧回调（YUV/RGB） | P0 |
| BYPASS-003 | 解码后音频帧回调（PCM） | P0 |
| BYPASS-004 | 渲染前视频帧回调（允许修改） | P1 |
| BYPASS-005 | 编码后数据回调 | P1 |

### 2.2 非功能需求

#### 2.2.1 性能指标

| 指标项 | 目标值 | 说明 |
|--------|--------|------|
| 本地文件首帧时间 | ≤ 500ms | 从调用play到首帧渲染 |
| 网络流首帧时间 | ≤ 2s | 含缓冲时间 |
| 直播延迟 | ≤ 3s | RTMP/HLS端到端延迟 |
| Seek响应时间 | ≤ 300ms | 本地文件 |
| 音视频同步偏差 | ≤ 40ms | 符合人耳感知阈值 |
| 1080p@30fps播放 | 稳定30fps | CPU占用<30% |

#### 2.2.2 兼容性要求

| 平台 | 最低版本 |
|------|----------|
| iOS | iOS 12.0+ |
| iPadOS | iPadOS 12.0+ |
| Android | API 21 (Android 5.0) |
| macOS | macOS 10.14+ |
| Windows | Windows 10 1803+ |

#### 2.2.3 稳定性要求

| 指标项 | 目标值 |
|--------|--------|
| 崩溃率 | < 0.1% |
| 播放成功率 | > 99% |
| 连续播放时长 | > 24h |

### 2.3 平台特定需求

#### iOS/iPadOS
- 支持应用沙盒内文件访问
- 支持后台音频播放
- 支持Control Center集成
- 麦克风/相机权限管理

#### Android
- 支持API 21-34兼容性
- 支持64位架构（arm64-v8a, x86_64）
- 支持前台服务权限
- 音频焦点管理

#### macOS
- 支持Apple Silicon（M1/M2/M3）和Intel芯片
- 支持Metal渲染
- 支持沙盒模式

#### Windows
- 支持x64架构
- 支持Direct3D 11渲染
- 支持高DPI适配

---

## 3. 架构设计文档

### 3.1 分层架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        应用层 (Application Layer)                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐  │
│  │  iOS App    │  │ Android App │  │  macOS App  │  │Windows  │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘  │
└─────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                    平台适配层 (Platform Adapter Layer)              │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              平台抽象接口 (Platform Abstraction)           │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │    │
│  │  │ 渲染抽象  │  │ 编解码抽象 │  │ 网络抽象  │  │ 采集抽象  │ │    │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │    │
│  └─────────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              平台具体实现 (Implementation)                 │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │    │
│  │  │iOS(Metal)│  │Android   │  │macOS     │  │Windows   │ │    │
│  │  │VideoToolbox│ │(GLES/MC) │  │(Metal/VT)│  │(DX/DXVA) │ │    │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                          核心层 (Core Layer)                        │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  媒体处理引擎 (Media Engine)                │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │    │
│  │  │ Demuxer  │  │ Decoder  │  │ Encoder  │  │  Muxer   │ │    │
│  │  │ 解封装   │  │  解码    │  │  编码    │  │  封装    │ │    │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │    │
│  │  │ Renderer │  │Resampler │  │  Filter  │  │  Clock   │ │    │
│  │  │  渲染    │  │ 重采样   │  │  滤镜    │  │  同步    │ │    │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                      基础设施层 (Infrastructure)                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │ 线程池   │  │ 内存池   │  │ 日志系统 │  │ 事件系统 │        │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘        │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 核心模块设计

#### 3.2.1 Demuxer（解封装模块）

```cpp
class IDemuxer {
public:
    virtual int Open(const std::string& url) = 0;
    virtual void Close() = 0;
    virtual int ReadPacket(AVPacket* packet) = 0;
    virtual int Seek(int64_t timestamp) = 0;
    virtual const MediaInfo& GetMediaInfo() const = 0;
};
```

#### 3.2.2 Decoder（解码模块）

```cpp
class IDecoder {
public:
    virtual int Initialize(const AVCodecParameters* params, DecoderType type) = 0;
    virtual int SendPacket(const AVPacket* packet) = 0;
    virtual int ReceiveFrame(AVFrame* frame) = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;
};
```

#### 3.2.3 Renderer（渲染模块）

```cpp
class IRenderer {
public:
    virtual int Initialize(void* native_window, int width, int height) = 0;
    virtual int RenderFrame(const AVFrame* frame) = 0;
    virtual void SetRenderMode(RenderMode mode) = 0;
    virtual void Release() = 0;
};
```

### 3.3 线程模型

```
主线程 (Main Thread)
    │
    ├──▶ 应用UI线程
    │     - 处理用户交互
    │     - 调用SDK API
    │
    └──▶ SDK控制线程
          - 状态管理
          - 回调分发

工作线程池 (Worker Thread Pool)
    │
    ├──▶ 解封装线程
    ├──▶ 视频解码线程
    ├──▶ 音频解码线程
    ├──▶ 渲染线程
    ├──▶ 音频播放线程
    └──▶ 网络IO线程

定时器线程
    └──▶ 时钟同步线程
```

### 3.4 内存管理策略

- **内存池设计**：预分配固定大小的内存块，减少运行时分配开销
- **引用计数机制**：使用智能指针管理FFmpeg对象生命周期
- **零拷贝优化**：硬件解码后直接渲染，避免CPU内存拷贝

---

## 4. 接口设计文档

### 4.1 C++核心公共API

```cpp
namespace AVSDK {

// 播放器接口
class IPlayer {
public:
    // 生命周期
    virtual ErrorCode Initialize(const PlayerConfig& config) = 0;
    virtual void Release() = 0;
    
    // 播放控制
    virtual ErrorCode SetDataSource(const std::string& url) = 0;
    virtual ErrorCode Prepare() = 0;
    virtual ErrorCode Start() = 0;
    virtual ErrorCode Pause() = 0;
    virtual ErrorCode Stop() = 0;
    virtual ErrorCode SeekTo(Timestamp positionMs) = 0;
    
    // 状态查询
    virtual PlayerState GetState() const = 0;
    virtual Timestamp GetCurrentPosition() const = 0;
    virtual Timestamp GetDuration() const = 0;
    
    // 渲染器设置
    virtual void SetVideoRenderer(std::shared_ptr<IVideoRenderer> renderer) = 0;
    virtual void SetAudioRenderer(std::shared_ptr<IAudioRenderer> renderer) = 0;
    
    // 回调设置
    virtual void SetCallback(IPlayerCallback* callback) = 0;
};

// 播放器工厂
class PlayerFactory {
public:
    static std::shared_ptr<IPlayer> CreatePlayer();
    static VersionInfo GetVersion();
    static ErrorCode Initialize();
    static void Terminate();
};

} // namespace AVSDK
```

### 4.2 iOS/macOS平台接口

```objc
// Objective-C播放器接口
@interface AVSDKPlayer : NSObject

@property (nonatomic, weak) id<AVSDKPlayerDelegate> delegate;
@property (nonatomic, assign, readonly) AVSDKPlayerState state;
@property (nonatomic, assign, readonly) NSTimeInterval currentTime;
@property (nonatomic, assign, readonly) NSTimeInterval duration;

- (instancetype)initWithConfig:(AVSDKPlayerConfig *)config;
- (BOOL)setDataSource:(NSString *)url error:(NSError **)error;
- (void)prepare;
- (BOOL)start:(NSError **)error;
- (BOOL)pause:(NSError **)error;
- (BOOL)stop:(NSError **)error;
- (void)seekTo:(NSTimeInterval)time;
- (void)attachToView:(UIView *)view;

+ (NSString *)versionString;
+ (BOOL)initializeSDK:(NSError **)error;

@end
```

### 4.3 Android平台接口

```java
public class AVSDKPlayer {
    
    public enum State {
        IDLE, INITIALIZING, READY, BUFFERING, 
        PLAYING, PAUSED, SEEKING, STOPPED, ERROR, RELEASED
    }
    
    public interface Callback {
        void onStateChanged(State oldState, State newState);
        void onPrepared(MediaInfo mediaInfo);
        void onCompletion();
        void onBufferingUpdate(int percent);
        void onProgressUpdate(long currentMs, long durationMs);
        void onError(AVSDKError error);
    }
    
    public AVSDKPlayer(Context context, Config config);
    public boolean setDataSource(String url);
    public void prepare();
    public boolean start();
    public boolean pause();
    public boolean stop();
    public void seekTo(long positionMs);
    public void setSurface(Surface surface);
    public void setPlayerListener(Callback listener);
}
```

### 4.4 错误码定义

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0 | OK | 成功 |
| 1 | Unknown | 未知错误 |
| 2 | InvalidParameter | 无效参数 |
| 4 | OutOfMemory | 内存不足 |
| 100 | PlayerOpenFailed | 打开媒体失败 |
| 200 | CodecNotFound | 编解码器未找到 |
| 300 | NetworkConnectFailed | 网络连接失败 |
| 400 | FileNotFound | 文件不存在 |
| 500 | HardwareNotAvailable | 硬件不可用 |

---

## 5. FFmpeg技术实现方案

### 5.1 FFmpeg版本选择

**推荐版本：FFmpeg 6.1.x LTS**

选择理由：
1. 稳定性：6.x系列是长期支持版本
2. H265支持：原生支持HEVC/H265解码
3. 硬解码完善：VideoToolbox、MediaCodec、DXVA/D3D11VA支持成熟
4. API稳定：相比7.x，6.x API更稳定

### 5.2 硬解码方案

| 平台 | 硬解码API | 支持格式 |
|------|-----------|----------|
| iOS/macOS | VideoToolbox | H264/H265/ProRes |
| Android | MediaCodec | H264/H265/VP8/VP9 |
| Windows | D3D11VA | H264/H265/VP9 |

#### VideoToolbox硬解码初始化

```cpp
bool VideoToolboxDecoder::Initialize(AVCodecContext* codec_ctx) {
    // 1. 创建格式描述
    CreateFormatDescription(codec_ctx);
    
    // 2. 配置解码器属性
    CFDictionaryRef decoder_spec = CreateDecoderSpec();
    
    // 3. 设置输出像素格式
    CFArrayRef pixel_formats = CreatePixelFormatArray();
    
    // 4. 创建解码会话
    VTDecompressionSessionCreate(
        nullptr, format_desc_, decoder_spec,
        output_image_buffer_attrs, &callback_record, &session_
    );
    
    return status == noErr;
}
```

#### MediaCodec硬解码初始化

```cpp
bool MediaCodecDecoder::Initialize(AVCodecContext* codec_ctx) {
    // 1. 确定编解码器名称
    const char* mime = (codec_ctx->codec_id == AV_CODEC_ID_H264) 
                       ? "video/avc" : "video/hevc";
    
    // 2. 创建MediaCodec
    codec_ = AMediaCodec_createDecoderByType(mime);
    
    // 3. 配置格式
    format_ = AMediaFormat_new();
    AMediaFormat_setString(format_, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(format_, AMEDIAFORMAT_KEY_WIDTH, codec_ctx->width);
    AMediaFormat_setInt32(format_, AMEDIAFORMAT_KEY_HEIGHT, codec_ctx->height);
    
    // 4. 配置并启动
    AMediaCodec_configure(codec_, format_, native_window_, nullptr, 0);
    AMediaCodec_start(codec_);
    
    return true;
}
```

### 5.3 渲染方案

| 平台 | 渲染API | 推荐度 |
|------|---------|--------|
| iOS | Metal | ★★★★★ |
| Android | OpenGL ES 3.0 | ★★★★★ |
| macOS | Metal | ★★★★★ |
| Windows | DirectX 11 | ★★★★★ |

#### Metal YUV到RGB转换着色器

```metal
// 顶点着色器
vertex VertexOut vertexShader(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.position = float4(in.position, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

// 片段着色器 - YUV420P to RGB
fragment float4 fragmentShaderYUV(VertexOut in [[stage_in]],
                                   texture2d<float> yTexture [[texture(0)]],
                                   texture2d<float> uvTexture [[texture(1)]]) {
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);
    
    float y = yTexture.sample(textureSampler, in.texCoord).r;
    float2 uv = uvTexture.sample(textureSampler, in.texCoord).rg - 0.5;
    
    float r = y + 1.402 * uv.y;
    float g = y - 0.344136 * uv.x - 0.714136 * uv.y;
    float b = y + 1.772 * uv.x;
    
    return float4(r, g, b, 1.0);
}
```

### 5.4 性能优化

#### 零拷贝优化

```
传统方式（有拷贝）:
解码器(GPU) → 系统内存(CPU拷贝) → 纹理上传(CPU-GPU) → GPU渲染

零拷贝方式:
解码器(GPU) → GPU纹理(共享) → GPU渲染
```

#### 缓冲策略

| 策略 | 适用场景 | 缓冲时间 |
|------|----------|----------|
| LowLatency | 直播 | 300ms |
| Smooth | 点播 | 5s |
| Balanced | 通用 | 1s |

---

## 6. 开发路线图

### 6.1 四阶段实现计划

```
第一阶段：本地播放 (Phase 1: Local Playback)
    │
    ├──▶ 核心模块开发
    │     - Demuxer实现
    │     - Decoder实现（软解码）
    │     - Renderer实现（Metal for macOS）
    │     - 音频播放实现
    │
    ├──▶ 基础设施
    │     - 线程池
    │     - 内存池
    │     - 日志系统
    │
    └──▶ 验证平台：macOS
          - Metal渲染
          - 本地文件播放
          - 基础播放控制

第二阶段：网络流播放 (Phase 2: Network Streaming)
    │
    ├──▶ 网络模块
    │     - HTTP/HTTPS支持
    │     - HLS支持
    │     - RTMP支持
    │     - 缓冲管理
    │
    ├──▶ 硬解码支持
    │     - VideoToolbox (macOS/iOS)
    │     - MediaCodec (Android)
    │     - DXVA/D3D11VA (Windows)
    │
    └──▶ 扩展平台：iOS、Android

第三阶段：实时编码 (Phase 3: Real-time Encoding)
    │
    ├──▶ 编码模块
    │     - H.264/H.265编码
    │     - 硬件编码支持
    │     - 码率控制
    │
    ├──▶ 采集模块
    │     - 摄像头采集
    │     - 屏幕采集
    │     - 麦克风采集
    │
    └──▶ 扩展平台：Windows

第四阶段：数据旁路回调 (Phase 4: Data Bypass)
    │
    ├──▶ 数据旁路模块
    │     - 原始视频帧回调
    │     - 原始音频帧回调
    │     - 编码后数据回调
    │
    └──▶ 滤镜支持
          - FFmpeg滤镜图
          - 自定义处理接口
```

### 6.2 各阶段交付物

| 阶段 | 交付物 | 验收标准 |
|------|--------|----------|
| 第一阶段 | macOS Demo + SDK | 1080p播放CPU<50%，首帧<1s |
| 第二阶段 | 网络Demo + SDK | RTMP/HLS正常播放，直播延迟<5s |
| 第三阶段 | 推流Demo + SDK | 1080p硬编码@30fps |
| 第四阶段 | 旁路Demo + SDK | 旁路不影响播放性能 |

---

## 7. 总结

### 7.1 核心技术选型

| 组件 | 技术选择 |
|------|----------|
| 底层框架 | FFmpeg 6.1.x |
| 开发语言 | C++11/14 |
| iOS/macOS硬解码 | VideoToolbox |
| Android硬解码 | MediaCodec |
| Windows硬解码 | D3D11VA |
| iOS/macOS渲染 | Metal |
| Android渲染 | OpenGL ES 3.0 |
| Windows渲染 | DirectX 11 |

### 7.2 项目优势

1. **跨平台支持**：一套C++核心代码支持四个主流平台
2. **硬件加速**：充分利用各平台硬件编解码能力
3. **H265支持**：支持最新的H265/HEVC编解码
4. **数据旁路**：支持AI处理、自定义滤镜等高级功能
5. **性能优化**：零拷贝、内存池、线程池等优化手段

### 7.3 参考文档

- FFmpeg官方文档: https://ffmpeg.org/documentation.html
- VideoToolbox文档: Apple Developer Documentation
- MediaCodec文档: Android Developer Documentation
- Metal文档: Apple Developer Documentation
- DirectX文档: Microsoft Documentation

---

**文档结束**

*详细内容请参考各子文档：*
- [详细需求文档](./跨平台音视频SDK_PRD.md)
- [详细架构设计](./media_sdk_architecture_design.md)
- [详细接口设计](./av_sdk_interface_design.md)
- [详细技术实现](./ffmpeg_sdk_technical_solution.md)
