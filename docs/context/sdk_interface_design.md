# HorsAVSDK 公共接口设计

**状态**: Active  
**创建日期**: 2026-04-21  
**最后更新**: 2026-04-21  
**版本**: v1.0

---

## 1. 设计原则

### 1.1 接口设计目标

1. **简单性**: 用户只需了解 5-6 个核心方法即可播放视频
2. **平台无关**: C++ 接口在所有平台（macOS/iOS/Android/Windows）保持一致
3. **可扩展性**: 通过配置和回调支持高级功能
4. **类型安全**: 使用强类型，避免 void* 滥用

### 1.2 错误处理策略

- 使用 `ErrorCode` 枚举返回错误
- 所有可能失败的操作返回 `ErrorCode`
- 成功返回 `ErrorCode::OK`

---

## 2. 核心接口

### 2.1 IPlayer (播放器)

**头文件**: `include/avsdk/player.h`

```cpp
namespace avsdk {

class IPlayer {
public:
    virtual ~IPlayer() = default;

    // ========== 生命周期 ==========
    
    /// 初始化播放器
    /// @param config 播放器配置（硬件解码、缓冲时间等）
    /// @return ErrorCode::OK 表示成功
    virtual ErrorCode Initialize(const PlayerConfig& config) = 0;
    
    /// 打开媒体文件
    /// @param url 文件路径或网络 URL
    /// @return ErrorCode::OK 表示成功
    virtual ErrorCode Open(const std::string& url) = 0;
    
    /// 关闭播放器，释放资源
    virtual void Close() = 0;

    // ========== 播放控制 ==========
    
    /// 开始播放
    virtual ErrorCode Play() = 0;
    
    /// 暂停播放
    virtual ErrorCode Pause() = 0;
    
    /// 停止播放（回到就绪状态）
    virtual ErrorCode Stop() = 0;
    
    /// 跳转到指定位置
    /// @param position_ms 目标位置（毫秒）
    virtual ErrorCode Seek(Timestamp position_ms) = 0;

    // ========== 渲染设置 ==========
    
    /// 设置渲染视图（平台相关）
    /// @param native_window macOS: MTKView*, iOS: UIView*, Android: Surface*
    virtual void SetRenderView(void* native_window) = 0;
    
    /// 设置自定义渲染器（可选，默认使用平台渲染器）
    virtual ErrorCode SetRenderer(std::shared_ptr<IRenderer> renderer) = 0;

    // ========== 状态查询 ==========
    
    /// 获取当前播放状态
    virtual PlayerState GetState() const = 0;
    
    /// 获取媒体信息（时长、宽高、码率等）
    virtual MediaInfo GetMediaInfo() const = 0;
    
    /// 获取当前播放位置（毫秒）
    virtual Timestamp GetCurrentPosition() const = 0;
    
    /// 获取总时长（毫秒）
    virtual Timestamp GetDuration() const = 0;
    
    // ========== 数据旁路（可选） ==========
    
    /// 注册数据旁路处理器
    virtual ErrorCode SetDataBypass(std::shared_ptr<IDataBypass> bypass) = 0;
    
    /// 启用/禁用视频帧回调
    virtual void EnableVideoFrameCallback(bool enable) = 0;
    
    /// 启用/禁用音频帧回调
    virtual void EnableAudioFrameCallback(bool enable) = 0;
};

/// 创建播放器实例
std::unique_ptr<IPlayer> CreatePlayer();

}
```

### 2.2 使用示例

```cpp
#include "avsdk/player.h"
#include "avsdk/player_config.h"

using namespace avsdk;

// 1. 创建播放器
auto player = CreatePlayer();

// 2. 配置并初始化
PlayerConfig config;
config.enable_hardware_decoder = true;
config.buffer_time_ms = 1000;
player->Initialize(config);

// 3. 设置渲染视图（macOS 示例，传入 MTKView）
player->SetRenderView((__bridge void*)mtkView);

// 4. 打开文件
if (player->Open("/path/to/video.mp4") == ErrorCode::OK) {
    // 5. 开始播放
    player->Play();
    
    // 6. 查询信息
    MediaInfo info = player->GetMediaInfo();
    std::cout << "Duration: " << info.duration_ms << " ms\n";
    std::cout << "Video: " << info.video_width << "x" << info.video_height << "\n";
}

// 7. 清理
player->Close();
```

---

## 3. 配置结构

### 3.1 PlayerConfig

**头文件**: `include/avsdk/player_config.h`

```cpp
struct PlayerConfig {
    // 解码设置
    bool enable_hardware_decoder = true;    // 启用硬件解码
    bool prefer_software_decoder = false;   // 优先使用软件解码
    
    // 缓冲设置
    int buffer_time_ms = 1000;              // 缓冲时长（毫秒）
    int max_buffer_size_mb = 50;            // 最大缓冲区大小（MB）
    
    // 网络设置
    int connect_timeout_ms = 10000;         // 连接超时
    int read_timeout_ms = 30000;            // 读取超时
    bool enable_caching = true;             // 启用本地缓存
    
    // 音频设置
    int audio_volume = 100;                 // 音量 (0-100)
    bool enable_audio = true;               // 启用音频
    
    // 日志设置
    LogLevel log_level = LogLevel::kInfo;   // 日志级别
};
```

### 3.2 MediaInfo

```cpp
struct MediaInfo {
    // 基本信息
    std::string url;                // 文件路径/URL
    std::string format;             // 容器格式（mp4, mkv 等）
    int64_t duration_ms = 0;        // 总时长（毫秒）
    int64_t bitrate = 0;            // 码率（bps）
    
    // 视频信息
    bool has_video = false;
    int video_width = 0;
    int video_height = 0;
    double video_framerate = 0;
    std::string video_codec;        // 视频编码格式
    
    // 音频信息
    bool has_audio = false;
    int audio_channels = 0;
    int audio_sample_rate = 0;
    std::string audio_codec;        // 音频编码格式
};
```

---

## 4. 类型定义

### 4.1 核心类型

**头文件**: `include/avsdk/types.h`

```cpp
namespace avsdk {

// 时间戳（毫秒）
using Timestamp = int64_t;

// 播放状态
enum class PlayerState {
    kIdle,      // 空闲（未初始化）
    kStopped,   // 停止（已初始化但未播放）
    kReady,     // 就绪（文件已打开）
    kPlaying,   // 播放中
    kPaused,    // 暂停
    kError      // 错误
};

// 错误码
enum class ErrorCode {
    OK = 0,                     // 成功
    
    // 通用错误 (1xxx)
    Unknown = 1000,
    InvalidParameter = 1001,
    OutOfMemory = 1002,
    NotInitialized = 1003,
    AlreadyInitialized = 1004,
    
    // 播放器错误 (2xxx)
    PlayerNotReady = 2000,
    PlayerAlreadyPlaying = 2001,
    PlayerSeekFailed = 2002,
    
    // 编解码错误 (3xxx)
    CodecNotFound = 3000,
    CodecNotSupported = 3001,
    CodecOpenFailed = 3002,
    CodecDecodeFailed = 3003,
    
    // 网络错误 (4xxx)
    NetworkConnectFailed = 4000,
    NetworkTimeout = 4001,
    NetworkDisconnected = 4002,
    
    // 文件错误 (5xxx)
    FileNotFound = 5000,
    FileOpenFailed = 5001,
    FileInvalidFormat = 5002,
    FileReadFailed = 5003,
    
    // 硬件错误 (6xxx)
    HardwareNotAvailable = 6000,
    HardwareInitFailed = 6001,
};

// 视频帧（用于回调）
struct VideoFrame {
    uint8_t* data[3];           // YUV 平面数据
    int linesize[3];            // 每行字节数
    int width;
    int height;
    int format;                 // AVPixelFormat
    int64_t pts;                // 显示时间戳
};

// 音频帧（用于回调）
struct AudioFrame {
    uint8_t* data;
    int size;                   // 数据大小（字节）
    int sample_rate;
    int channels;
    int format;                 // AVSampleFormat
    int64_t pts;
};

}
```

---

## 5. 内部接口（供平台实现）

### 5.1 IDemuxer (解封装器)

**头文件**: `include/avsdk/demuxer.h`

```cpp
class IDemuxer {
public:
    virtual ~IDemuxer() = default;
    
    virtual ErrorCode Open(const std::string& url) = 0;
    virtual void Close() = 0;
    
    /// 读取一个媒体包
    /// @return nullptr 表示流结束
    virtual AVPacketPtr ReadPacket() = 0;
    
    /// 跳转到指定位置
    virtual ErrorCode Seek(Timestamp position_ms) = 0;
    
    /// 获取媒体信息
    virtual MediaInfo GetMediaInfo() const = 0;
    
    /// 获取视频流索引
    virtual int GetVideoStreamIndex() const = 0;
    
    /// 获取音频流索引
    virtual int GetAudioStreamIndex() const = 0;
    
    /// 获取视频编码参数（用于初始化解码器）
    virtual AVCodecParameters* GetVideoCodecParameters() const = 0;
};

// 工厂函数
std::unique_ptr<IDemuxer> CreateFFmpegDemuxer();
std::unique_ptr<IDemuxer> CreateNetworkDemuxer();
```

### 5.2 IDecoder (解码器)

**头文件**: `include/avsdk/decoder.h`

```cpp
class IDecoder {
public:
    virtual ~IDecoder() = default;
    
    /// 初始化解码器
    /// @param codecpar FFmpeg 编码参数（包含 codec_id, extradata 等）
    virtual ErrorCode Initialize(AVCodecParameters* codecpar) = 0;
    
    /// 解码一个包
    /// @return nullptr 表示需要更多数据或解码失败
    virtual AVFramePtr Decode(const AVPacketPtr& packet) = 0;
    
    /// 刷新解码器（清空缓冲）
    virtual void Flush() = 0;
    
    /// 关闭解码器
    virtual void Close() = 0;
};

// 工厂函数
std::unique_ptr<IDecoder> CreateFFmpegDecoder();
```

### 5.3 IRenderer (视频渲染器)

**头文件**: `include/avsdk/renderer.h`

```cpp
class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    /// 初始化渲染器
    /// @param native_window 平台相关窗口句柄
    /// @param width 视频宽度
    /// @param height 视频高度
    virtual ErrorCode Initialize(void* native_window, int width, int height) = 0;
    
    /// 渲染一帧
    virtual ErrorCode RenderFrame(const AVFrame* frame) = 0;
    
    /// 释放资源
    virtual void Release() = 0;
};

// 平台工厂函数
std::unique_ptr<IRenderer> CreatePlatformRenderer();  // 自动选择平台实现
```

### 5.4 IAudioRenderer (音频渲染器)

**头文件**: `include/avsdk/audio_renderer.h`

```cpp
enum class RendererState {
    kStopped,
    kPlaying,
    kPaused
};

struct AudioFormat {
    int sample_rate;        // 采样率（Hz）
    int channels;           // 声道数
    int bits_per_sample;    // 位深（16/24/32）
};

class IAudioRenderer {
public:
    virtual ~IAudioRenderer() = default;
    
    virtual ErrorCode Initialize(const AudioFormat& format) = 0;
    virtual void Close() = 0;
    
    virtual ErrorCode Play() = 0;
    virtual ErrorCode Pause() = 0;
    virtual ErrorCode Stop() = 0;
    
    /// 写入音频数据
    /// @return 实际写入的字节数
    virtual int WriteAudio(const uint8_t* data, size_t size) = 0;
    
    /// 获取已缓冲的音频时长（毫秒）
    virtual int GetBufferedDuration() const = 0;
    
    virtual RendererState GetState() const = 0;
    virtual int GetVolume() const = 0;
    virtual void SetVolume(int volume) = 0;
};
```

---

## 6. 回调接口

### 6.1 IDataBypass (数据旁路)

**头文件**: `include/avsdk/data_bypass.h`

```cpp
class IDataBypass {
public:
    virtual ~IDataBypass() = default;
    
    /// 原始编码包（解封装后，解码前）
    virtual void OnRawPacket(const EncodedPacket& packet) = 0;
    
    /// 解码后的视频帧
    virtual void OnDecodedVideoFrame(const VideoFrame& frame) = 0;
    
    /// 解码后的音频帧
    virtual void OnDecodedAudioFrame(const AudioFrame& frame) = 0;
    
    /// 渲染前的视频帧（可修改）
    virtual void OnPreRenderVideoFrame(VideoFrame& frame) = 0;
    
    /// 渲染前的音频帧（可修改）
    virtual void OnPreRenderAudioFrame(AudioFrame& frame) = 0;
    
    /// 编码后的包（用于录制）
    virtual void OnEncodedPacket(const EncodedPacket& packet) = 0;
};
```

### 6.2 使用场景

| 回调 | 时机 | 用途 |
|------|------|------|
| OnRawPacket | 解封装后 | 录制原始流 |
| OnDecodedVideoFrame | 解码后 | 截图、分析 |
| OnDecodedAudioFrame | 解码后 | 音频处理 |
| OnPreRenderVideoFrame | 渲染前 | 滤镜、水印 |
| OnEncodedPacket | 编码后 | 录制转码 |

---

## 7. FFmpeg 对象封装

### 7.1 智能指针定义

```cpp
// FFmpeg 对象使用自定义删除器的 shared_ptr
using AVFramePtr = std::shared_ptr<AVFrame>;
using AVPacketPtr = std::shared_ptr<AVPacket>;

// 创建示例
AVFramePtr frame(av_frame_alloc(), 
                 [](AVFrame* f) { av_frame_free(&f); });

AVPacketPtr packet(av_packet_alloc(),
                   [](AVPacket* p) { av_packet_free(&p); });
```

### 7.2 为什么使用 shared_ptr

1. **自动内存管理**: 避免内存泄漏
2. **引用计数**: 支持多线程共享
3. **异常安全**: 异常时自动释放
4. **一致性**: 所有 FFmpeg 对象统一使用

---

## 8. 平台适配

### 8.1 平台特定类型

| 平台 | native_window 类型 | 渲染实现 |
|------|-------------------|---------|
| macOS | `MTKView*` | `MetalRenderer` |
| iOS | `UIView*` (内部创建 MTKView) | `MetalRenderer` |
| Android | `Surface*` or `SurfaceTexture*` | `GLESRenderer` |
| Windows | `HWND` | `D3DRenderer` |

### 8.2 macOS 示例

```cpp
// Objective-C++
#import <MetalKit/MetalKit.h>

MTKView* mtkView = [[MTKView alloc] initWithFrame:frame];
player->SetRenderView((__bridge void*)mtkView);
```

### 8.3 Android 示例

```cpp
// JNI
extern "C" JNIEXPORT void JNICALL
Java_com_avsdk_Player_setSurface(JNIEnv* env, jobject thiz, jobject surface) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    player->SetRenderView(window);
}
```

---

## 9. 错误处理最佳实践

### 9.1 错误码检查

```cpp
auto result = player->Open("video.mp4");
if (result != ErrorCode::OK) {
    std::string msg = GetErrorString(result);
    LOG_ERROR("Player", "Open failed: " + msg);
    
    // 针对特定错误处理
    switch (result) {
        case ErrorCode::FileNotFound:
            ShowUserMessage("文件不存在");
            break;
        case ErrorCode::CodecNotSupported:
            ShowUserMessage("不支持的编码格式");
            break;
        default:
            ShowUserMessage("播放失败: " + msg);
    }
    return;
}
```

### 9.2 错误码转换字符串

```cpp
const char* GetErrorString(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "Success";
        case ErrorCode::FileNotFound: return "File not found";
        case ErrorCode::CodecNotFound: return "Codec not found";
        // ... 其他错误码
        default: return "Unknown error";
    }
}
```

---

## 10. 版本兼容性

### 10.1 向后兼容性策略

1. **接口冻结**: 核心接口 (IPlayer) 一旦发布，保持兼容
2. **扩展方式**: 新增功能通过 `PlayerConfig` 或新接口实现
3. **废弃策略**: 废弃接口保留但标记为 deprecated

### 10.2 版本检查

```cpp
// 获取 SDK 版本
const char* GetSDKVersion();  // e.g., "1.2.3"
int GetSDKVersionMajor();
int GetSDKVersionMinor();
int GetSDKVersionPatch();
```

---

## 11. 参考

- [SDK 架构文档](sdk_architecture_current.md)
- [PlayerImpl 实现详解](player_impl_design.md)
- [Metal 渲染策略](metal_rendering_strategy.md)
