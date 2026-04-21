# 跨平台音视频SDK接口设计文档

## 版本信息
- **版本**: v1.0.0
- **日期**: 2024年
- **支持平台**: iOS/iPadOS 12.0+, Android 6.0+, macOS 10.14+, Windows 10+
- **核心依赖**: FFmpeg 6.0+

---

## 目录
1. [架构概述](#1-架构概述)
2. [C++核心公共API](#2-c核心公共api)
3. [iOS/macOS平台接口](#3-iosmacos平台接口)
4. [Android平台接口](#4-android平台接口)
5. [Windows平台接口](#5-windows平台接口)
6. [错误码定义](#6-错误码定义)
7. [配置参数详解](#7-配置参数详解)
8. [使用示例](#8-使用示例)

---

## 1. 架构概述

### 1.1 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      应用层 (Application)                        │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │ iOS/macOS   │  │   Android   │  │   Windows   │  │  其他   │ │
│  │  Obj-C/Swift│  │  Java/Kotlin│  │   C++/C#    │  │  平台   │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └────┬────┘ │
│         │                │                │              │      │
│  ┌──────┴──────┐  ┌──────┴──────┐  ┌──────┴──────┐  ┌────┴────┐ │
│  │  Platform   │  │  Platform   │  │  Platform   │  │Platform │ │
│  │   Bridge    │  │   Bridge    │  │   Bridge    │  │ Bridge  │ │
│  │ (Obj-C++)   │  │   (JNI)     │  │  (C++/CLI)  │  │  (...)  │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └────┬────┘ │
├─────────┼────────────────┼────────────────┼──────────────┼──────┤
│         └────────────────┴────────────────┴──────────────┘      │
│                         C++ 核心层                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │   Player    │  │   Encoder   │  │   Decoder   │  │  Utils  │ │
│  │   Core      │  │    Core     │  │    Core     │  │  Core   │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └────┬────┘ │
│         └────────────────┴────────────────┴──────────────┘      │
│                         FFmpeg 层                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │  libavcodec │  │ libavformat │  │  libswscale │  │  ...    │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                      硬件抽象层 (HAL)                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │ VideoToolbox│  │  MediaCodec │  │  DXVA/D3D   │  │  ...    │ │
│  │  (Apple)    │  │  (Android)  │  │  (Windows)  │  │         │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 核心设计原则

1. **平台无关性**: C++核心层不依赖任何平台特定API
2. **接口一致性**: 各平台包装层保持API语义一致
3. **可扩展性**: 支持未来添加新平台和功能
4. **性能优先**: 关键路径零拷贝，硬件加速优先

---

## 2. C++核心公共API

### 2.1 命名空间

```cpp
namespace AVSDK {
    // 所有公共API都在此命名空间下
}
```

### 2.2 基础类型定义

```cpp
// 文件: include/avsdk/types.h

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace AVSDK {

// 版本信息
struct VersionInfo {
    uint32_t major;      // 主版本号
    uint32_t minor;      // 次版本号
    uint32_t patch;      // 修订版本号
    std::string build;   // 构建信息
    
    std::string toString() const {
        return std::to_string(major) + "." + 
               std::to_string(minor) + "." + 
               std::to_string(patch);
    }
};

// 视频分辨率
struct VideoResolution {
    int width;
    int height;
    
    bool operator==(const VideoResolution& other) const {
        return width == other.width && height == other.height;
    }
};

// 时间戳（毫秒）
using Timestamp = int64_t;

// 时间范围
struct TimeRange {
    Timestamp start;  // 开始时间（毫秒）
    Timestamp end;    // 结束时间（毫秒）
    
    Timestamp duration() const { return end - start; }
};

// 媒体信息
struct MediaInfo {
    std::string url;                    // 媒体URL
    std::string format;                 // 容器格式
    int64_t fileSize;                   // 文件大小（字节）
    Timestamp duration;                 // 总时长（毫秒）
    double bitrate;                     // 总码率（bps）
    
    // 视频信息
    bool hasVideo;
    VideoResolution videoResolution;    // 视频分辨率
    double videoFrameRate;              // 帧率
    std::string videoCodec;             // 视频编码格式
    int videoBitrate;                   // 视频码率
    
    // 音频信息
    bool hasAudio;
    int audioSampleRate;                // 音频采样率
    int audioChannels;                  // 音频通道数
    std::string audioCodec;             // 音频编码格式
    int audioBitrate;                   // 音频码率
};

// 视频帧数据
struct VideoFrame {
    uint8_t* data[4];           // YUV/RGB数据指针
    int linesize[4];            // 每行字节数
    VideoResolution resolution; // 分辨率
    int format;                 // 像素格式 (AVPixelFormat)
    Timestamp pts;              // 显示时间戳
    Timestamp dts;              // 解码时间戳
    int64_t pos;                // 文件位置
    bool keyFrame;              // 是否关键帧
    
    // 硬件加速相关
    void* hwContext;            // 硬件上下文
    int hwFormat;               // 硬件像素格式
};

// 音频帧数据
struct AudioFrame {
    uint8_t** data;             // 音频数据指针数组
    int samples;                // 每通道采样数
    int format;                 // 采样格式 (AVSampleFormat)
    int sampleRate;             // 采样率
    int channels;               // 通道数
    uint64_t channelLayout;     // 通道布局
    Timestamp pts;              // 时间戳
};

// 编码数据包
struct EncodedPacket {
    uint8_t* data;              // 编码数据
    int size;                   // 数据大小
    Timestamp pts;              // 显示时间戳
    Timestamp dts;              // 解码时间戳
    bool keyFrame;              // 是否关键帧
    int streamIndex;            // 流索引
    int flags;                  // 标志位
};

} // namespace AVSDK
```

### 2.3 错误码定义

```cpp
// 文件: include/avsdk/error.h

#pragma once
#include <cstdint>
#include <string>

namespace AVSDK {

// 错误码枚举
enum class ErrorCode : int32_t {
    // 成功
    OK = 0,
    
    // 通用错误 (1-99)
    Unknown = 1,                // 未知错误
    InvalidParameter = 2,       // 无效参数
    NullPointer = 3,            // 空指针
    OutOfMemory = 4,            // 内存不足
    NotImplemented = 5,         // 未实现
    NotInitialized = 6,         // 未初始化
    AlreadyInitialized = 7,     // 已初始化
    OperationNotAllowed = 8,    // 操作不允许
    Timeout = 9,                // 超时
    Cancelled = 10,             // 已取消
    
    // 播放器错误 (100-199)
    PlayerNotCreated = 100,     // 播放器未创建
    PlayerAlreadyPlaying = 101, // 播放器已在播放
    PlayerNotPlaying = 102,     // 播放器未在播放
    PlayerOpenFailed = 103,     // 打开媒体失败
    PlayerSeekFailed = 104,     // 跳转失败
    PlayerNoVideoStream = 105,  // 无视频流
    PlayerNoAudioStream = 106,  // 无音频流
    PlayerRenderFailed = 107,   // 渲染失败
    PlayerBufferFull = 108,     // 缓冲区满
    PlayerBufferEmpty = 109,    // 缓冲区空
    PlayerEndOfStream = 110,    // 流结束
    
    // 编解码器错误 (200-299)
    CodecNotFound = 200,        // 编解码器未找到
    CodecOpenFailed = 201,      // 打开编解码器失败
    CodecDecodeFailed = 202,    // 解码失败
    CodecEncodeFailed = 203,    // 编码失败
    CodecNotSupported = 204,    // 不支持的编解码器
    CodecHardwareFailed = 205,  // 硬件编解码失败
    CodecSoftwareFallback = 206,// 回退到软件编解码
    
    // 网络错误 (300-399)
    NetworkConnectFailed = 300, // 连接失败
    NetworkDisconnected = 301,  // 连接断开
    NetworkTimeout = 302,       // 网络超时
    NetworkResolveFailed = 303, // 解析域名失败
    NetworkInvalidURL = 304,    // 无效URL
    NetworkIOError = 305,       // 网络IO错误
    NetworkHttpError = 306,     // HTTP错误
    
    // 文件错误 (400-499)
    FileNotFound = 400,         // 文件不存在
    FileOpenFailed = 401,       // 打开文件失败
    FileReadFailed = 402,       // 读取文件失败
    FileWriteFailed = 403,      // 写入文件失败
    FileInvalidFormat = 404,    // 无效文件格式
    FileCorrupted = 405,        // 文件损坏
    
    // 硬件错误 (500-599)
    HardwareNotAvailable = 500, // 硬件不可用
    HardwareInitFailed = 501,   // 硬件初始化失败
    HardwareSurfaceFailed = 502,// 硬件表面创建失败
    HardwareContextFailed = 503,// 硬件上下文失败
    
    // DRM错误 (600-699)
    DRMNotSupported = 600,      // DRM不支持
    DRMLicenseFailed = 601,     // 许可证获取失败
    DRMDecryptFailed = 602,     // 解密失败
};

// 错误信息结构
struct ErrorInfo {
    ErrorCode code;             // 错误码
    std::string message;        // 错误描述
    std::string detail;         // 详细错误信息
    int32_t systemError;        // 系统错误码
    
    ErrorInfo() : code(ErrorCode::OK), systemError(0) {}
    ErrorInfo(ErrorCode c, const std::string& m) 
        : code(c), message(m), systemError(0) {}
};

// 获取错误码对应的错误信息
const char* GetErrorString(ErrorCode code);

} // namespace AVSDK
```

### 2.4 播放器配置

```cpp
// 文件: include/avsdk/player_config.h

#pragma once
#include "types.h"
#include "error.h"

namespace AVSDK {

// 解码方式
enum class DecodeMode {
    Auto,           // 自动选择（优先硬件）
    Software,       // 强制软件解码
    Hardware,       // 强制硬件解码
    HardwareFirst,  // 优先硬件，失败回退软件
};

// 缓冲策略
enum class BufferStrategy {
    LowLatency,     // 低延迟（直播场景）
    Smooth,         // 流畅优先（点播场景）
    Balanced,       // 平衡模式
    Custom,         // 自定义
};

// 视频缩放模式
enum class ScaleMode {
    Fit,            // 适应（保持比例，完整显示）
    Fill,           // 填充（保持比例，填满视图）
    Stretch,        // 拉伸（不保持比例）
    Center,         // 居中（原始大小）
};

// 播放器配置
struct PlayerConfig {
    // 解码配置
    DecodeMode decodeMode = DecodeMode::Auto;
    bool enableHardwareDecoder = true;      // 启用硬件解码器
    bool enableH265Decoder = true;          // 启用H265解码
    bool enableMultiThreadDecode = true;    // 启用多线程解码
    int maxDecodeThreads = 4;               // 最大解码线程数
    
    // 缓冲配置
    BufferStrategy bufferStrategy = BufferStrategy::Balanced;
    int bufferTimeMs = 1000;                // 缓冲时间（毫秒）
    int minBufferTimeMs = 500;              // 最小缓冲时间
    int maxBufferTimeMs = 5000;             // 最大缓冲时间
    int maxBufferSize = 50 * 1024 * 1024;   // 最大缓冲区大小（50MB）
    
    // 网络配置
    int connectTimeoutMs = 10000;           // 连接超时（毫秒）
    int readTimeoutMs = 30000;              // 读取超时（毫秒）
    int retryCount = 3;                     // 重试次数
    int retryIntervalMs = 1000;             // 重试间隔
    bool enableHttpKeepAlive = true;        // HTTP Keep-Alive
    std::string userAgent;                  // User-Agent
    std::vector<std::string> headers;       // 自定义HTTP头
    
    // 视频配置
    ScaleMode scaleMode = ScaleMode::Fit;
    bool enableDeinterlace = false;         // 去隔行
    bool enableHdr = true;                  // HDR支持
    
    // 音频配置
    int audioVolume = 100;                  // 音量 (0-100)
    bool audioMute = false;                 // 静音
    int audioLatencyMs = 0;                 // 音频延迟调整
    
    // 字幕配置
    bool enableSubtitle = true;             // 启用字幕
    std::string preferredSubtitleLang;      // 首选字幕语言
    
    // 调试配置
    bool enableLog = false;                 // 启用日志
    int logLevel = 2;                       // 日志级别 (0-5)
    std::string logPath;                    // 日志文件路径
    
    // 验证配置
    ErrorCode Validate() const;
};

} // namespace AVSDK
```

### 2.5 播放器状态

```cpp
// 文件: include/avsdk/player_state.h

#pragma once

namespace AVSDK {

// 播放器状态
enum class PlayerState {
    Idle,           // 空闲
    Initializing,   // 初始化中
    Ready,          // 准备就绪
    Buffering,      // 缓冲中
    Playing,        // 播放中
    Paused,         // 暂停
    Seeking,        // 跳转中
    Stopped,        // 停止
    Error,          // 错误状态
    Released,       // 已释放
};

// 播放事件类型
enum class PlayerEvent {
    // 状态事件
    StateChanged,       // 状态变化
    
    // 播放事件
    Prepared,           // 准备完成
    Started,            // 开始播放
    Paused,             // 暂停
    Resumed,            // 恢复
    Stopped,            // 停止
    Completed,          // 播放完成
    SeekCompleted,      // 跳转完成
    
    // 缓冲事件
    BufferingStart,     // 开始缓冲
    BufferingEnd,       // 结束缓冲
    BufferingProgress,  // 缓冲进度
    
    // 视频事件
    VideoSizeChanged,   // 视频尺寸变化
    VideoFrameRendered, // 视频帧渲染
    FirstFrameRendered, // 首帧渲染
    
    // 音频事件
    AudioFormatChanged, // 音频格式变化
    
    // 错误事件
    Error,              // 错误
    Warning,            // 警告
    
    // 元数据事件
    MediaInfoUpdated,   // 媒体信息更新
    MetadataReceived,   // 元数据接收
};

// 播放器事件数据
struct PlayerEventData {
    PlayerEvent event;
    PlayerState oldState;
    PlayerState newState;
    int64_t param1;     // 通用参数1
    int64_t param2;     // 通用参数2
    double progress;    // 进度 (0.0 - 1.0)
    ErrorInfo error;    // 错误信息
    void* userData;     // 用户数据
};

} // namespace AVSDK
```

### 2.6 播放器回调接口

```cpp
// 文件: include/avsdk/player_callback.h

#pragma once
#include "types.h"
#include "player_state.h"

namespace AVSDK {

// 播放器事件回调接口
class IPlayerCallback {
public:
    virtual ~IPlayerCallback() = default;
    
    // 状态变化回调
    virtual void OnStateChanged(PlayerState oldState, PlayerState newState) = 0;
    
    // 准备完成回调
    virtual void OnPrepared(const MediaInfo& info) = 0;
    
    // 播放完成回调
    virtual void OnCompletion() = 0;
    
    // 缓冲回调
    virtual void OnBufferingStart() = 0;
    virtual void OnBufferingEnd() = 0;
    virtual void OnBufferingUpdate(int percent) = 0;  // 缓冲进度 0-100
    
    // 进度回调
    virtual void OnProgressUpdate(Timestamp currentMs, Timestamp durationMs) = 0;
    
    // 视频尺寸变化回调
    virtual void OnVideoSizeChanged(int width, int height) = 0;
    
    // 首帧渲染回调
    virtual void OnFirstFrameRendered() = 0;
    
    // 错误回调
    virtual void OnError(const ErrorInfo& error) = 0;
    
    // 警告回调
    virtual void OnWarning(const ErrorInfo& warning) = 0;
    
    // 元数据回调
    virtual void OnMetadata(const std::string& key, const std::string& value) = 0;
    
    // 数据旁路回调（第四阶段）
    virtual void OnVideoFrame(const VideoFrame& frame) { }
    virtual void OnAudioFrame(const AudioFrame& frame) { }
    virtual void OnEncodedPacket(const EncodedPacket& packet) { }
};

// 播放器回调接口（使用std::function版本）
struct PlayerCallbacks {
    std::function<void(PlayerState, PlayerState)> onStateChanged;
    std::function<void(const MediaInfo&)> onPrepared;
    std::function<void()> onCompletion;
    std::function<void()> onBufferingStart;
    std::function<void()> onBufferingEnd;
    std::function<void(int)> onBufferingUpdate;
    std::function<void(Timestamp, Timestamp)> onProgressUpdate;
    std::function<void(int, int)> onVideoSizeChanged;
    std::function<void()> onFirstFrameRendered;
    std::function<void(const ErrorInfo&)> onError;
    std::function<void(const ErrorInfo&)> onWarning;
    std::function<void(const std::string&, const std::string&)> onMetadata;
    std::function<void(const VideoFrame&)> onVideoFrame;
    std::function<void(const AudioFrame&)> onAudioFrame;
    std::function<void(const EncodedPacket&)> onEncodedPacket;
};

} // namespace AVSDK
```

### 2.7 播放器核心类

```cpp
// 文件: include/avsdk/player.h

#pragma once
#include "types.h"
#include "error.h"
#include "player_config.h"
#include "player_callback.h"

namespace AVSDK {

// 渲染接口（平台实现）
class IVideoRenderer {
public:
    virtual ~IVideoRenderer() = default;
    
    // 初始化渲染器
    virtual ErrorCode Initialize(void* nativeView) = 0;
    
    // 释放渲染器
    virtual void Release() = 0;
    
    // 渲染视频帧
    virtual ErrorCode RenderFrame(const VideoFrame& frame) = 0;
    
    // 清空渲染
    virtual void Clear() = 0;
    
    // 设置缩放模式
    virtual void SetScaleMode(ScaleMode mode) = 0;
    
    // 设置渲染区域
    virtual void SetRenderRect(int x, int y, int width, int height) = 0;
    
    // 截图
    virtual ErrorCode CaptureFrame(std::vector<uint8_t>& data, int& width, int& height) = 0;
};

// 音频渲染接口
class IAudioRenderer {
public:
    virtual ~IAudioRenderer() = default;
    
    // 初始化音频渲染
    virtual ErrorCode Initialize(int sampleRate, int channels, int format) = 0;
    
    // 释放
    virtual void Release() = 0;
    
    // 播放音频帧
    virtual ErrorCode PlayFrame(const AudioFrame& frame) = 0;
    
    // 控制
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;
    virtual void Flush() = 0;
    
    // 音量控制
    virtual void SetVolume(int volume) = 0;
    virtual int GetVolume() const = 0;
    
    // 延迟
    virtual int GetLatencyMs() const = 0;
};

// 播放器接口
class IPlayer {
public:
    virtual ~IPlayer() = default;
    
    // ==================== 生命周期 ====================
    
    // 初始化播放器
    // @param config: 播放器配置
    // @return: 错误码
    virtual ErrorCode Initialize(const PlayerConfig& config) = 0;
    
    // 释放播放器资源
    virtual void Release() = 0;
    
    // ==================== 设置回调 ====================
    
    // 设置事件回调接口
    // @param callback: 回调接口指针（播放器不管理生命周期）
    virtual void SetCallback(IPlayerCallback* callback) = 0;
    
    // 设置回调函数集合
    // @param callbacks: 回调函数集合
    virtual void SetCallbacks(const PlayerCallbacks& callbacks) = 0;
    
    // ==================== 数据源 ====================
    
    // 设置数据源（本地文件或网络URL）
    // @param url: 媒体文件路径或URL
    // @return: 错误码
    virtual ErrorCode SetDataSource(const std::string& url) = 0;
    
    // 设置自定义数据源（高级用法）
    // @param dataSource: 自定义数据源接口
    // virtual ErrorCode SetDataSource(std::shared_ptr<IDataSource> dataSource) = 0;
    
    // ==================== 播放控制 ====================
    
    // 准备播放（异步）
    // 调用后会开始加载媒体信息，完成后触发OnPrepared回调
    // @return: 错误码
    virtual ErrorCode Prepare() = 0;
    
    // 准备播放（同步）
    // @return: 错误码
    virtual ErrorCode PrepareSync() = 0;
    
    // 开始播放
    // @return: 错误码
    virtual ErrorCode Start() = 0;
    
    // 暂停播放
    // @return: 错误码
    virtual ErrorCode Pause() = 0;
    
    // 恢复播放
    // @return: 错误码
    virtual ErrorCode Resume() = 0;
    
    // 停止播放
    // @return: 错误码
    virtual ErrorCode Stop() = 0;
    
    // 重置播放器
    // @return: 错误码
    virtual ErrorCode Reset() = 0;
    
    // ==================== 跳转定位 ====================
    
    // 跳转到指定位置（异步）
    // @param positionMs: 目标位置（毫秒）
    // @return: 错误码
    virtual ErrorCode SeekTo(Timestamp positionMs) = 0;
    
    // 跳转到指定位置（同步）
    // @param positionMs: 目标位置（毫秒）
    // @return: 错误码
    virtual ErrorCode SeekToSync(Timestamp positionMs) = 0;
    
    // 快进/快退
    // @param offsetMs: 偏移量（毫秒，正数为快进）
    // @return: 错误码
    virtual ErrorCode SeekBy(Timestamp offsetMs) = 0;
    
    // ==================== 播放参数 ====================
    
    // 设置播放速度
    // @param speed: 播放速度 (0.5 - 2.0)
    // @return: 错误码
    virtual ErrorCode SetPlaybackSpeed(float speed) = 0;
    
    // 获取播放速度
    // @return: 当前播放速度
    virtual float GetPlaybackSpeed() const = 0;
    
    // 设置循环播放
    // @param loop: 是否循环
    virtual void SetLooping(bool loop) = 0;
    
    // 是否循环播放
    // @return: 是否循环
    virtual bool IsLooping() const = 0;
    
    // ==================== 渲染器设置 ====================
    
    // 设置视频渲染器
    // @param renderer: 视频渲染器
    virtual void SetVideoRenderer(std::shared_ptr<IVideoRenderer> renderer) = 0;
    
    // 设置音频渲染器
    // @param renderer: 音频渲染器
    virtual void SetAudioRenderer(std::shared_ptr<IAudioRenderer> renderer) = 0;
    
    // ==================== 状态查询 ====================
    
    // 获取当前状态
    // @return: 播放器状态
    virtual PlayerState GetState() const = 0;
    
    // 获取当前播放位置
    // @return: 当前位置（毫秒）
    virtual Timestamp GetCurrentPosition() const = 0;
    
    // 获取总时长
    // @return: 总时长（毫秒）
    virtual Timestamp GetDuration() const = 0;
    
    // 获取缓冲位置
    // @return: 缓冲位置（毫秒）
    virtual Timestamp GetBufferedPosition() const = 0;
    
    // 获取缓冲进度
    // @return: 缓冲进度 (0-100)
    virtual int GetBufferPercentage() const = 0;
    
    // 是否正在播放
    // @return: 是否播放中
    virtual bool IsPlaying() const = 0;
    
    // 是否已准备
    // @return: 是否已准备
    virtual bool IsPrepared() const = 0;
    
    // ==================== 媒体信息 ====================
    
    // 获取媒体信息
    // @return: 媒体信息
    virtual MediaInfo GetMediaInfo() const = 0;
    
    // 获取视频宽度
    // @return: 视频宽度
    virtual int GetVideoWidth() const = 0;
    
    // 获取视频高度
    // @return: 视频高度
    virtual int GetVideoHeight() const = 0;
    
    // ==================== 音频控制 ====================
    
    // 设置音量
    // @param volume: 音量 (0-100)
    virtual void SetVolume(int volume) = 0;
    
    // 获取音量
    // @return: 当前音量
    virtual int GetVolume() const = 0;
    
    // 设置静音
    // @param mute: 是否静音
    virtual void SetMute(bool mute) = 0;
    
    // 是否静音
    // @return: 是否静音
    virtual bool IsMute() const = 0;
    
    // ==================== 视频控制 ====================
    
    // 设置缩放模式
    // @param mode: 缩放模式
    virtual void SetScaleMode(ScaleMode mode) = 0;
    
    // 获取缩放模式
    // @return: 当前缩放模式
    virtual ScaleMode GetScaleMode() const = 0;
    
    // 截图
    // @param data: 输出图像数据 (RGB24)
    // @param width: 输出图像宽度
    // @param height: 输出图像高度
    // @return: 错误码
    virtual ErrorCode CaptureFrame(std::vector<uint8_t>& data, int& width, int& height) = 0;
    
    // ==================== 轨道选择 ====================
    
    // 获取视频轨道数量
    // @return: 视频轨道数
    virtual int GetVideoTrackCount() const = 0;
    
    // 获取音频轨道数量
    // @return: 音频轨道数
    virtual int GetAudioTrackCount() const = 0;
    
    // 获取字幕轨道数量
    // @return: 字幕轨道数
    virtual int GetSubtitleTrackCount() const = 0;
    
    // 选择视频轨道
    // @param index: 轨道索引
    // @return: 错误码
    virtual ErrorCode SelectVideoTrack(int index) = 0;
    
    // 选择音频轨道
    // @param index: 轨道索引
    // @return: 错误码
    virtual ErrorCode SelectAudioTrack(int index) = 0;
    
    // 选择字幕轨道
    // @param index: 轨道索引 (-1 禁用字幕)
    // @return: 错误码
    virtual ErrorCode SelectSubtitleTrack(int index) = 0;
    
    // ==================== 数据旁路（第四阶段） ====================
    
    // 启用原始视频帧回调
    // @param enable: 是否启用
    virtual void EnableVideoFrameCallback(bool enable) = 0;
    
    // 启用原始音频帧回调
    // @param enable: 是否启用
    virtual void EnableAudioFrameCallback(bool enable) = 0;
    
    // 启用编码数据回调
    // @param enable: 是否启用
    virtual void EnableEncodedPacketCallback(bool enable) = 0;
    
    // 设置数据回调格式
    // @param videoFormat: 视频像素格式 (AVPixelFormat)
    // @param audioFormat: 音频采样格式 (AVSampleFormat)
    virtual void SetCallbackFormat(int videoFormat, int audioFormat) = 0;
};

// 播放器工厂函数
class PlayerFactory {
public:
    // 创建播放器实例
    // @return: 播放器实例
    static std::shared_ptr<IPlayer> CreatePlayer();
    
    // 获取SDK版本
    // @return: 版本信息
    static VersionInfo GetVersion();
    
    // 初始化SDK全局资源
    // @return: 错误码
    static ErrorCode Initialize();
    
    // 释放SDK全局资源
    static void Terminate();
    
    // 设置全局日志回调
    // @param callback: 日志回调函数
    static void SetLogCallback(std::function<void(int, const std::string&)> callback);
    
    // 设置全局日志级别
    // @param level: 日志级别 (0-5)
    static void SetLogLevel(int level);
};

} // namespace AVSDK
```

### 2.8 编码器配置

```cpp
// 文件: include/avsdk/encoder_config.h

#pragma once
#include "types.h"

namespace AVSDK {

// 视频编码器类型
enum class VideoEncoderType {
    Auto,           // 自动选择
    H264,           // H.264/AVC
    H265,           // H.265/HEVC
    VP8,            // VP8
    VP9,            // VP9
    AV1,            // AV1
};

// 编码器实现
enum class EncoderImplementation {
    Auto,           // 自动选择
    Software,       // 软件编码
    Hardware,       // 硬件编码
};

// 码率控制模式
enum class BitrateControlMode {
    CBR,            // 恒定码率
    VBR,            // 可变码率
    CQ,             // 恒定质量
    CRF,            // 恒定速率因子 (x264/x265)
};

// 编码配置
struct EncoderConfig {
    // 视频编码配置
    VideoEncoderType videoEncoderType = VideoEncoderType::H264;
    EncoderImplementation encoderImpl = EncoderImplementation::Auto;
    VideoResolution resolution = {1920, 1080};
    int frameRate = 30;
    int videoBitrate = 4000000;         // 4 Mbps
    BitrateControlMode bitrateMode = BitrateControlMode::VBR;
    int videoQuality = 23;              // CRF质量 (0-51, 越小越好)
    int keyFrameInterval = 2;           // 关键帧间隔（秒）
    int bFrames = 3;                    // B帧数量
    bool enableHwAcceleration = true;   // 启用硬件加速
    
    // 音频编码配置
    bool enableAudio = true;
    int audioSampleRate = 48000;
    int audioChannels = 2;
    int audioBitrate = 128000;          // 128 kbps
    std::string audioCodec = "aac";     // 音频编码器
    
    // 性能配置
    int encoderThreads = 4;             // 编码线程数
    int encoderPreset = 3;              // 编码预设 (0-9, ultrafast到veryslow)
    
    // 验证配置
    ErrorCode Validate() const;
};

} // namespace AVSDK
```

### 2.9 编码器核心类

```cpp
// 文件: include/avsdk/encoder.h

#pragma once
#include "types.h"
#include "error.h"
#include "encoder_config.h"

namespace AVSDK {

// 编码器回调接口
class IEncoderCallback {
public:
    virtual ~IEncoderCallback() = default;
    
    // 编码数据输出回调
    virtual void OnEncodedData(const EncodedPacket& packet) = 0;
    
    // 编码错误回调
    virtual void OnEncodeError(const ErrorInfo& error) = 0;
    
    // 编码器就绪回调
    virtual void OnEncoderReady() = 0;
    
    // 编码器停止回调
    virtual void OnEncoderStopped() = 0;
};

// 编码器接口
class IEncoder {
public:
    virtual ~IEncoder() = default;
    
    // ==================== 生命周期 ====================
    
    // 初始化编码器
    // @param config: 编码配置
    // @return: 错误码
    virtual ErrorCode Initialize(const EncoderConfig& config) = 0;
    
    // 释放编码器资源
    virtual void Release() = 0;
    
    // ==================== 回调设置 ====================
    
    // 设置回调接口
    // @param callback: 回调接口指针
    virtual void SetCallback(IEncoderCallback* callback) = 0;
    
    // ==================== 编码控制 ====================
    
    // 开始编码
    // @return: 错误码
    virtual ErrorCode Start() = 0;
    
    // 停止编码
    // @return: 错误码
    virtual ErrorCode Stop() = 0;
    
    // 刷新编码器（输出缓冲的帧）
    // @return: 错误码
    virtual ErrorCode Flush() = 0;
    
    // ==================== 视频编码 ====================
    
    // 编码视频帧
    // @param frame: 视频帧数据
    // @return: 错误码
    virtual ErrorCode EncodeVideoFrame(const VideoFrame& frame) = 0;
    
    // 编码视频帧（原始数据）
    // @param data: 原始数据指针
    // @param width: 宽度
    // @param height: 高度
    // @param format: 像素格式
    // @param timestamp: 时间戳
    // @return: 错误码
    virtual ErrorCode EncodeVideoFrame(const uint8_t* data, int width, int height, 
                                        int format, Timestamp timestamp) = 0;
    
    // ==================== 音频编码 ====================
    
    // 编码音频帧
    // @param frame: 音频帧数据
    // @return: 错误码
    virtual ErrorCode EncodeAudioFrame(const AudioFrame& frame) = 0;
    
    // 编码音频帧（原始数据）
    // @param data: 原始数据指针
    // @param samples: 采样数
    // @param format: 采样格式
    // @param timestamp: 时间戳
    // @return: 错误码
    virtual ErrorCode EncodeAudioFrame(const uint8_t* data, int samples, 
                                        int format, Timestamp timestamp) = 0;
    
    // ==================== 动态参数调整 ====================
    
    // 动态调整码率
    // @param bitrate: 新码率
    // @return: 错误码
    virtual ErrorCode SetBitrate(int bitrate) = 0;
    
    // 动态调整帧率
    // @param frameRate: 新帧率
    // @return: 错误码
    virtual ErrorCode SetFrameRate(int frameRate) = 0;
    
    // 动态调整分辨率
    // @param width: 新宽度
    // @param height: 新高度
    // @return: 错误码
    virtual ErrorCode SetResolution(int width, int height) = 0;
    
    // ==================== 状态查询 ====================
    
    // 是否正在编码
    // @return: 是否编码中
    virtual bool IsEncoding() const = 0;
    
    // 获取编码统计信息
    // @param encodedFrames: 已编码帧数
    // @param droppedFrames: 丢弃帧数
    // @param encodedBytes: 已编码字节数
    virtual void GetStats(int64_t& encodedFrames, int64_t& droppedFrames, 
                          int64_t& encodedBytes) const = 0;
    
    // 获取编码延迟
    // @return: 编码延迟（毫秒）
    virtual int GetLatencyMs() const = 0;
};

// 编码器工厂
class EncoderFactory {
public:
    // 创建编码器实例
    // @return: 编码器实例
    static std::shared_ptr<IEncoder> CreateEncoder();
    
    // 获取支持的编码器列表
    // @return: 支持的编码器类型列表
    static std::vector<VideoEncoderType> GetSupportedEncoders();
    
    // 检查是否支持硬件编码
    // @param type: 编码器类型
    // @return: 是否支持硬件编码
    static bool IsHardwareEncodingSupported(VideoEncoderType type);
};

} // namespace AVSDK
```

### 2.10 工具类

```cpp
// 文件: include/avsdk/utils.h

#pragma once
#include "types.h"
#include "error.h"

namespace AVSDK {

// 时间工具
class TimeUtil {
public:
    // 获取当前时间戳（毫秒）
    static Timestamp GetCurrentTimeMs();
    
    // 获取当前时间戳（微秒）
    static int64_t GetCurrentTimeUs();
    
    // 时间戳转字符串
    static std::string FormatTimestamp(Timestamp ms);
    
    // 时间戳转时分秒
    static void ToHMS(Timestamp ms, int& hours, int& minutes, int& seconds);
};

// 日志工具
enum class LogLevel {
    Verbose = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Fatal = 5,
};

class Logger {
public:
    // 设置日志级别
    static void SetLevel(LogLevel level);
    
    // 设置日志回调
    static void SetCallback(std::function<void(LogLevel, const std::string&)> callback);
    
    // 设置日志文件
    static void SetLogFile(const std::string& path);
    
    // 日志输出
    static void Log(LogLevel level, const std::string& tag, const std::string& message);
    static void V(const std::string& tag, const std::string& message);
    static void D(const std::string& tag, const std::string& message);
    static void I(const std::string& tag, const std::string& message);
    static void W(const std::string& tag, const std::string& message);
    static void E(const std::string& tag, const std::string& message);
};

// 线程工具
class ThreadUtil {
public:
    // 设置线程名称
    static void SetThreadName(const std::string& name);
    
    // 获取线程ID
    static uint64_t GetThreadId();
};

// 内存池（用于减少内存分配）
class MemoryPool {
public:
    // 分配内存
    static uint8_t* Allocate(size_t size);
    
    // 释放内存
    static void Free(uint8_t* ptr);
    
    // 释放池
    static void Clear();
};

} // namespace AVSDK
```

---

## 3. iOS/macOS平台接口

### 3.1 Objective-C播放器接口

```objc
// 文件: AVSDKPlayer.h

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>  // iOS
// #import <AppKit/AppKit.h>  // macOS

NS_ASSUME_NONNULL_BEGIN

// 播放器状态
typedef NS_ENUM(NSInteger, AVSDKPlayerState) {
    AVSDKPlayerStateIdle = 0,
    AVSDKPlayerStateInitializing,
    AVSDKPlayerStateReady,
    AVSDKPlayerStateBuffering,
    AVSDKPlayerStatePlaying,
    AVSDKPlayerStatePaused,
    AVSDKPlayerStateSeeking,
    AVSDKPlayerStateStopped,
    AVSDKPlayerStateError,
    AVSDKPlayerStateReleased
};

// 解码模式
typedef NS_ENUM(NSInteger, AVSDKDecodeMode) {
    AVSDKDecodeModeAuto = 0,
    AVSDKDecodeModeSoftware,
    AVSDKDecodeModeHardware,
    AVSDKDecodeModeHardwareFirst
};

// 缓冲策略
typedef NS_ENUM(NSInteger, AVSDKBufferStrategy) {
    AVSDKBufferStrategyLowLatency = 0,
    AVSDKBufferStrategySmooth,
    AVSDKBufferStrategyBalanced,
    AVSDKBufferStrategyCustom
};

// 缩放模式
typedef NS_ENUM(NSInteger, AVSDKScaleMode) {
    AVSDKScaleModeFit = 0,
    AVSDKScaleModeFill,
    AVSDKScaleModeStretch,
    AVSDKScaleModeCenter
};

// 错误码
typedef NS_ENUM(NSInteger, AVSDKErrorCode) {
    AVSDKErrorCodeOK = 0,
    AVSDKErrorCodeUnknown = 1,
    AVSDKErrorCodeInvalidParameter = 2,
    AVSDKErrorCodeOutOfMemory = 4,
    AVSDKErrorCodeNotInitialized = 6,
    AVSDKErrorCodePlayerOpenFailed = 103,
    AVSDKErrorCodePlayerSeekFailed = 104,
    AVSDKErrorCodeNetworkConnectFailed = 300,
    AVSDKErrorCodeNetworkTimeout = 302,
    AVSDKErrorCodeFileNotFound = 400,
    AVSDKErrorCodeCodecNotSupported = 204,
    AVSDKErrorCodeHardwareNotAvailable = 500
};

// 前向声明
@class AVSDKPlayer;
@class AVSDKPlayerConfig;
@class AVSDKMediaInfo;
@class AVSDKError;
@class AVSDKVideoFrame;
@class AVSDKAudioFrame;

// 播放器委托协议
@protocol AVSDKPlayerDelegate <NSObject>

@optional
// 状态变化
- (void)player:(AVSDKPlayer *)player didChangeState:(AVSDKPlayerState)oldState toState:(AVSDKPlayerState)newState;

// 准备完成
- (void)player:(AVSDKPlayer *)player didPrepareWithMediaInfo:(AVSDKMediaInfo *)mediaInfo;

// 播放完成
- (void)playerDidComplete:(AVSDKPlayer *)player;

// 缓冲事件
- (void)playerDidStartBuffering:(AVSDKPlayer *)player;
- (void)playerDidEndBuffering:(AVSDKPlayer *)player;
- (void)player:(AVSDKPlayer *)player didUpdateBufferingProgress:(NSInteger)progress;

// 进度更新
- (void)player:(AVSDKPlayer *)player didUpdateProgress:(NSTimeInterval)currentTime duration:(NSTimeInterval)duration;

// 视频尺寸变化
- (void)player:(AVSDKPlayer *)player didChangeVideoSize:(CGSize)size;

// 首帧渲染
- (void)playerDidRenderFirstFrame:(AVSDKPlayer *)player;

// 错误
- (void)player:(AVSDKPlayer *)player didEncounterError:(AVSDKError *)error;

// 警告
- (void)player:(AVSDKPlayer *)player didReceiveWarning:(AVSDKError *)warning;

// 数据旁路回调（可选）
- (void)player:(AVSDKPlayer *)player didCaptureVideoFrame:(AVSDKVideoFrame *)frame;
- (void)player:(AVSDKPlayer *)player didCaptureAudioFrame:(AVSDKAudioFrame *)frame;

@end

// 播放器配置
@interface AVSDKPlayerConfig : NSObject <NSCopying>

// 解码配置
@property (nonatomic, assign) AVSDKDecodeMode decodeMode;
@property (nonatomic, assign) BOOL enableHardwareDecoder;
@property (nonatomic, assign) BOOL enableH265Decoder;
@property (nonatomic, assign) int maxDecodeThreads;

// 缓冲配置
@property (nonatomic, assign) AVSDKBufferStrategy bufferStrategy;
@property (nonatomic, assign) NSTimeInterval bufferTime;
@property (nonatomic, assign) NSTimeInterval minBufferTime;
@property (nonatomic, assign) NSTimeInterval maxBufferTime;

// 网络配置
@property (nonatomic, assign) NSTimeInterval connectTimeout;
@property (nonatomic, assign) NSTimeInterval readTimeout;
@property (nonatomic, assign) NSInteger retryCount;

// 视频配置
@property (nonatomic, assign) AVSDKScaleMode scaleMode;
@property (nonatomic, assign) BOOL enableHdr;

// 音频配置
@property (nonatomic, assign) NSInteger volume;
@property (nonatomic, assign) BOOL mute;

// 调试配置
@property (nonatomic, assign) BOOL enableLog;
@property (nonatomic, assign) NSInteger logLevel;

// 默认配置
+ (instancetype)defaultConfig;

@end

// 媒体信息
@interface AVSDKMediaInfo : NSObject

@property (nonatomic, copy, readonly) NSString *url;
@property (nonatomic, copy, readonly) NSString *format;
@property (nonatomic, assign, readonly) int64_t fileSize;
@property (nonatomic, assign, readonly) NSTimeInterval duration;
@property (nonatomic, assign, readonly) double bitrate;

// 视频信息
@property (nonatomic, assign, readonly) BOOL hasVideo;
@property (nonatomic, assign, readonly) NSInteger videoWidth;
@property (nonatomic, assign, readonly) NSInteger videoHeight;
@property (nonatomic, assign, readonly) double videoFrameRate;
@property (nonatomic, copy, readonly) NSString *videoCodec;

// 音频信息
@property (nonatomic, assign, readonly) BOOL hasAudio;
@property (nonatomic, assign, readonly) NSInteger audioSampleRate;
@property (nonatomic, assign, readonly) NSInteger audioChannels;
@property (nonatomic, copy, readonly) NSString *audioCodec;

@end

// 错误信息
@interface AVSDKError : NSObject

@property (nonatomic, assign, readonly) AVSDKErrorCode code;
@property (nonatomic, copy, readonly) NSString *message;
@property (nonatomic, copy, readonly) NSString *detail;

+ (instancetype)errorWithCode:(AVSDKErrorCode)code message:(NSString *)message;
- (NSString *)localizedDescription;

@end

// 视频帧数据（数据旁路）
@interface AVSDKVideoFrame : NSObject

@property (nonatomic, assign, readonly) uint8_t *data;
@property (nonatomic, assign, readonly) NSInteger width;
@property (nonatomic, assign, readonly) NSInteger height;
@property (nonatomic, assign, readonly) NSInteger format;
@property (nonatomic, assign, readonly) NSTimeInterval timestamp;
@property (nonatomic, assign, readonly) BOOL isKeyFrame;

@end

// 音频帧数据（数据旁路）
@interface AVSDKAudioFrame : NSObject

@property (nonatomic, assign, readonly) uint8_t *data;
@property (nonatomic, assign, readonly) NSInteger samples;
@property (nonatomic, assign, readonly) NSInteger sampleRate;
@property (nonatomic, assign, readonly) NSInteger channels;
@property (nonatomic, assign, readonly) NSTimeInterval timestamp;

@end

// 播放器主类
@interface AVSDKPlayer : NSObject

// 委托
@property (nonatomic, weak, nullable) id<AVSDKPlayerDelegate> delegate;

// 状态
@property (nonatomic, assign, readonly) AVSDKPlayerState state;
@property (nonatomic, assign, readonly, getter=isPlaying) BOOL playing;
@property (nonatomic, assign, readonly, getter=isPrepared) BOOL prepared;

// 时间
@property (nonatomic, assign, readonly) NSTimeInterval currentTime;
@property (nonatomic, assign, readonly) NSTimeInterval duration;
@property (nonatomic, assign, readonly) NSTimeInterval bufferedTime;

// 媒体信息
@property (nonatomic, strong, readonly, nullable) AVSDKMediaInfo *mediaInfo;

// 视频尺寸
@property (nonatomic, assign, readonly) CGSize videoSize;

// 播放控制
@property (nonatomic, assign) float playbackSpeed;
@property (nonatomic, assign) BOOL looping;
@property (nonatomic, assign) AVSDKScaleMode scaleMode;

// 音量控制
@property (nonatomic, assign) NSInteger volume;  // 0-100
@property (nonatomic, assign) BOOL muted;

// 初始化
- (instancetype)init;
- (instancetype)initWithConfig:(AVSDKPlayerConfig *)config;

// 数据源设置
- (BOOL)setDataSource:(NSString *)url error:(NSError **)error;
- (BOOL)setDataSourceWithURL:(NSURL *)url error:(NSError **)error;

// 准备
- (void)prepare;
- (BOOL)prepareSync:(NSError **)error;

// 播放控制
- (BOOL)start:(NSError **)error;
- (BOOL)pause:(NSError **)error;
- (BOOL)resume:(NSError **)error;
- (BOOL)stop:(NSError **)error;
- (BOOL)reset:(NSError **)error;

// 跳转
- (void)seekTo:(NSTimeInterval)time;
- (BOOL)seekToSync:(NSTimeInterval)time error:(NSError **)error;

// 渲染视图设置（iOS）
- (void)attachToView:(UIView *)view;
- (void)detachFromView;

// 截图
- (nullable UIImage *)captureFrame;

// 数据旁路控制
@property (nonatomic, assign) BOOL enableVideoFrameCallback;
@property (nonatomic, assign) BOOL enableAudioFrameCallback;

// 版本信息
+ (NSString *)versionString;
+ (NSString *)buildInfo;

// 全局初始化
+ (BOOL)initializeSDK:(NSError **)error;
+ (void)terminateSDK;

@end

NS_ASSUME_NONNULL_END
```

### 3.2 Objective-C实现文件（桥接层）

```objc
// 文件: AVSDKPlayer.mm

#import "AVSDKPlayer.h"
#include "avsdk/player.h"
#include "avsdk/player_config.h"
#include "avsdk/error.h"

using namespace AVSDK;

// C++回调桥接类
class PlayerCallbackBridge : public IPlayerCallback {
public:
    __weak AVSDKPlayer *player;
    
    void OnStateChanged(PlayerState oldState, PlayerState newState) override {
        // 桥接到Objective-C委托
        AVSDKPlayer *strongPlayer = player;
        if (strongPlayer && [strongPlayer.delegate respondsToSelector:@selector(player:didChangeState:toState:)]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongPlayer.delegate player:strongPlayer 
                               didChangeState:(AVSDKPlayerState)oldState 
                                      toState:(AVSDKPlayerState)newState];
            });
        }
    }
    
    void OnPrepared(const MediaInfo& info) override {
        // 桥接准备完成回调
    }
    
    void OnCompletion() override {
        AVSDKPlayer *strongPlayer = player;
        if (strongPlayer && [strongPlayer.delegate respondsToSelector:@selector(playerDidComplete:)]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongPlayer.delegate playerDidComplete:strongPlayer];
            });
        }
    }
    
    void OnBufferingStart() override {}
    void OnBufferingEnd() override {}
    void OnBufferingUpdate(int percent) override {}
    void OnProgressUpdate(Timestamp currentMs, Timestamp durationMs) override {}
    void OnVideoSizeChanged(int width, int height) override {}
    void OnFirstFrameRendered() override {}
    void OnError(const ErrorInfo& error) override {}
    void OnWarning(const ErrorInfo& warning) override {}
    void OnMetadata(const std::string& key, const std::string& value) override {}
};

@interface AVSDKPlayer () {
    std::shared_ptr<IPlayer> _player;
    std::shared_ptr<IVideoRenderer> _videoRenderer;
    std::unique_ptr<PlayerCallbackBridge> _callbackBridge;
}
@end

@implementation AVSDKPlayer

- (instancetype)init {
    return [self initWithConfig:[AVSDKPlayerConfig defaultConfig]];
}

- (instancetype)initWithConfig:(AVSDKPlayerConfig *)config {
    self = [super init];
    if (self) {
        _player = PlayerFactory::CreatePlayer();
        _callbackBridge = std::make_unique<PlayerCallbackBridge>();
        _callbackBridge->player = self;
        _player->SetCallback(_callbackBridge.get());
        
        // 应用配置
        [self applyConfig:config];
    }
    return self;
}

- (void)applyConfig:(AVSDKPlayerConfig *)config {
    PlayerConfig cppConfig;
    cppConfig.decodeMode = (DecodeMode)config.decodeMode;
    cppConfig.enableHardwareDecoder = config.enableHardwareDecoder;
    cppConfig.enableH265Decoder = config.enableH265Decoder;
    cppConfig.maxDecodeThreads = config.maxDecodeThreads;
    cppConfig.bufferStrategy = (BufferStrategy)config.bufferStrategy;
    cppConfig.bufferTimeMs = (int)(config.bufferTime * 1000);
    cppConfig.scaleMode = (ScaleMode)config.scaleMode;
    cppConfig.enableLog = config.enableLog;
    
    _player->Initialize(cppConfig);
}

- (void)attachToView:(UIView *)view {
    // 创建平台特定的渲染器
    _videoRenderer = CreateIOSVideoRenderer((__bridge void*)view);
    _player->SetVideoRenderer(_videoRenderer);
}

- (BOOL)start:(NSError **)error {
    auto result = _player->Start();
    if (result != ErrorCode::OK && error) {
        *error = [NSError errorWithDomain:@"AVSDK" 
                                     code:(NSInteger)result 
                                 userInfo:@{NSLocalizedDescriptionKey: @"Start failed"}];
    }
    return result == ErrorCode::OK;
}

// ... 其他方法实现

@end
```

### 3.3 Swift接口扩展

```swift
// 文件: AVSDKPlayer+Swift.swift

import Foundation

// MARK: - Swift友好的扩展

extension AVSDKPlayer {
    
    /// 异步准备播放器
    /// - Parameter completion: 准备完成回调
    func prepare(completion: @escaping (Result<AVSDKMediaInfo, AVSDKError>) -> Void) {
        // 设置一次性回调
        let delegate = SwiftPlayerDelegate(completion: completion)
        self.delegate = delegate
        self.prepare()
    }
    
    /// 异步跳转
    /// - Parameters:
    ///   - time: 目标时间
    ///   - completion: 跳转完成回调
    func seek(to time: TimeInterval, completion: @escaping (Bool) -> Void) {
        // 实现异步跳转
    }
    
    /// 播放（异步）
    /// - Returns: 播放结果
    func play() async throws -> Bool {
        return try await withCheckedThrowingContinuation { continuation in
            let success = self.start(nil)
            continuation.resume(returning: success)
        }
    }
    
    /// 当前进度（0.0 - 1.0）
    var progress: Double {
        guard duration > 0 else { return 0 }
        return currentTime / duration
    }
    
    /// 缓冲进度（0.0 - 1.0）
    var bufferProgress: Double {
        guard duration > 0 else { return 0 }
        return bufferedTime / duration
    }
}

// MARK: - 便捷配置

extension AVSDKPlayerConfig {
    
    /// 直播低延迟配置
    static var liveConfig: AVSDKPlayerConfig {
        let config = AVSDKPlayerConfig()
        config.bufferStrategy = .lowLatency
        config.bufferTime = 0.3
        config.minBufferTime = 0.1
        return config
    }
    
    /// 点播流畅配置
    static var vodConfig: AVSDKPlayerConfig {
        let config = AVSDKPlayerConfig()
        config.bufferStrategy = .smooth
        config.bufferTime = 5.0
        config.maxBufferTime = 10.0
        return config
    }
}

// MARK: - 委托包装

private class SwiftPlayerDelegate: NSObject, AVSDKPlayerDelegate {
    let completion: (Result<AVSDKMediaInfo, AVSDKError>) -> Void
    
    init(completion: @escaping (Result<AVSDKMediaInfo, AVSDKError>) -> Void) {
        self.completion = completion
    }
    
    func player(_ player: AVSDKPlayer, didPrepareWithMediaInfo mediaInfo: AVSDKMediaInfo) {
        completion(.success(mediaInfo))
    }
    
    func player(_ player: AVSDKPlayer, didEncounterError error: AVSDKError) {
        completion(.failure(error))
    }
}
```

---

## 4. Android平台接口

### 4.1 Java播放器接口

```java
// 文件: AVSDKPlayer.java

package com.avsdk.player;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.TextureView;
import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.List;
import java.util.Map;

/**
 * AVSDK播放器主类
 */
public class AVSDKPlayer {
    
    static {
        System.loadLibrary("avsdk");
    }
    
    // ==================== 枚举定义 ====================
    
    /**
     * 播放器状态
     */
    public enum State {
        IDLE(0),
        INITIALIZING(1),
        READY(2),
        BUFFERING(3),
        PLAYING(4),
        PAUSED(5),
        SEEKING(6),
        STOPPED(7),
        ERROR(8),
        RELEASED(9);
        
        private final int value;
        State(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    /**
     * 解码模式
     */
    public enum DecodeMode {
        AUTO(0),
        SOFTWARE(1),
        HARDWARE(2),
        HARDWARE_FIRST(3);
        
        private final int value;
        DecodeMode(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    /**
     * 缓冲策略
     */
    public enum BufferStrategy {
        LOW_LATENCY(0),
        SMOOTH(1),
        BALANCED(2),
        CUSTOM(3);
        
        private final int value;
        BufferStrategy(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    /**
     * 缩放模式
     */
    public enum ScaleMode {
        FIT(0),
        FILL(1),
        STRETCH(2),
        CENTER(3);
        
        private final int value;
        ScaleMode(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    /**
     * 错误码
     */
    public enum ErrorCode {
        OK(0),
        UNKNOWN(1),
        INVALID_PARAMETER(2),
        OUT_OF_MEMORY(4),
        NOT_INITIALIZED(6),
        PLAYER_OPEN_FAILED(103),
        PLAYER_SEEK_FAILED(104),
        NETWORK_CONNECT_FAILED(300),
        NETWORK_TIMEOUT(302),
        FILE_NOT_FOUND(400),
        CODEC_NOT_SUPPORTED(204),
        HARDWARE_NOT_AVAILABLE(500);
        
        private final int value;
        ErrorCode(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    // ==================== 回调接口 ====================
    
    /**
     * 播放器回调接口
     */
    public interface Callback {
        /**
         * 状态变化
         */
        default void onStateChanged(State oldState, State newState) {}
        
        /**
         * 准备完成
         */
        default void onPrepared(MediaInfo mediaInfo) {}
        
        /**
         * 播放完成
         */
        default void onCompletion() {}
        
        /**
         * 开始缓冲
         */
        default void onBufferingStart() {}
        
        /**
         * 结束缓冲
         */
        default void onBufferingEnd() {}
        
        /**
         * 缓冲进度更新
         * @param percent 缓冲进度 (0-100)
         */
        default void onBufferingUpdate(int percent) {}
        
        /**
         * 播放进度更新
         */
        default void onProgressUpdate(long currentMs, long durationMs) {}
        
        /**
         * 视频尺寸变化
         */
        default void onVideoSizeChanged(int width, int height) {}
        
        /**
         * 首帧渲染
         */
        default void onFirstFrameRendered() {}
        
        /**
         * 错误回调
         */
        default void onError(AVSDKError error) {}
        
        /**
         * 警告回调
         */
        default void onWarning(AVSDKError warning) {}
        
        /**
         * 元数据接收
         */
        default void onMetadata(String key, String value) {}
        
        /**
         * 视频帧数据回调（数据旁路）
         */
        default void onVideoFrame(VideoFrame frame) {}
        
        /**
         * 音频帧数据回调（数据旁路）
         */
        default void onAudioFrame(AudioFrame frame) {}
    }
    
    // ==================== 数据类 ====================
    
    /**
     * 播放器配置
     */
    public static class Config {
        public DecodeMode decodeMode = DecodeMode.AUTO;
        public boolean enableHardwareDecoder = true;
        public boolean enableH265Decoder = true;
        public int maxDecodeThreads = 4;
        
        public BufferStrategy bufferStrategy = BufferStrategy.BALANCED;
        public int bufferTimeMs = 1000;
        public int minBufferTimeMs = 500;
        public int maxBufferTimeMs = 5000;
        
        public int connectTimeoutMs = 10000;
        public int readTimeoutMs = 30000;
        public int retryCount = 3;
        
        public ScaleMode scaleMode = ScaleMode.FIT;
        public boolean enableHdr = true;
        
        public int volume = 100;
        public boolean mute = false;
        
        public boolean enableLog = false;
        public int logLevel = 2;
        
        public static Config defaultConfig() {
            return new Config();
        }
        
        public static Config liveConfig() {
            Config config = new Config();
            config.bufferStrategy = BufferStrategy.LOW_LATENCY;
            config.bufferTimeMs = 300;
            config.minBufferTimeMs = 100;
            return config;
        }
    }
    
    /**
     * 媒体信息
     */
    public static class MediaInfo {
        public final String url;
        public final String format;
        public final long fileSize;
        public final long durationMs;
        public final double bitrate;
        
        public final boolean hasVideo;
        public final int videoWidth;
        public final int videoHeight;
        public final double videoFrameRate;
        public final String videoCodec;
        public final int videoBitrate;
        
        public final boolean hasAudio;
        public final int audioSampleRate;
        public final int audioChannels;
        public final String audioCodec;
        public final int audioBitrate;
        
        public MediaInfo(String url, String format, long fileSize, long durationMs,
                        double bitrate, boolean hasVideo, int videoWidth, int videoHeight,
                        double videoFrameRate, String videoCodec, int videoBitrate,
                        boolean hasAudio, int audioSampleRate, int audioChannels,
                        String audioCodec, int audioBitrate) {
            this.url = url;
            this.format = format;
            this.fileSize = fileSize;
            this.durationMs = durationMs;
            this.bitrate = bitrate;
            this.hasVideo = hasVideo;
            this.videoWidth = videoWidth;
            this.videoHeight = videoHeight;
            this.videoFrameRate = videoFrameRate;
            this.videoCodec = videoCodec;
            this.videoBitrate = videoBitrate;
            this.hasAudio = hasAudio;
            this.audioSampleRate = audioSampleRate;
            this.audioChannels = audioChannels;
            this.audioCodec = audioCodec;
            this.audioBitrate = audioBitrate;
        }
    }
    
    /**
     * 错误信息
     */
    public static class AVSDKError {
        public final ErrorCode code;
        public final String message;
        public final String detail;
        
        public AVSDKError(ErrorCode code, String message, String detail) {
            this.code = code;
            this.message = message;
            this.detail = detail;
        }
        
        @Override
        public String toString() {
            return "AVSDKError{" +
                    "code=" + code +
                    ", message='" + message + '\'' +
                    ", detail='" + detail + '\'' +
                    '}';
        }
    }
    
    /**
     * 视频帧数据
     */
    public static class VideoFrame {
        public final byte[] data;
        public final int width;
        public final int height;
        public final int format;
        public final long timestampMs;
        public final boolean isKeyFrame;
        
        public VideoFrame(byte[] data, int width, int height, int format, 
                         long timestampMs, boolean isKeyFrame) {
            this.data = data;
            this.width = width;
            this.height = height;
            this.format = format;
            this.timestampMs = timestampMs;
            this.isKeyFrame = isKeyFrame;
        }
    }
    
    /**
     * 音频帧数据
     */
    public static class AudioFrame {
        public final byte[] data;
        public final int samples;
        public final int sampleRate;
        public final int channels;
        public final long timestampMs;
        
        public AudioFrame(byte[] data, int samples, int sampleRate, 
                         int channels, long timestampMs) {
            this.data = data;
            this.samples = samples;
            this.sampleRate = sampleRate;
            this.channels = channels;
            this.timestampMs = timestampMs;
        }
    }
    
    // ==================== 实例变量 ====================
    
    private long nativeHandle;  // C++对象指针
    private Callback callback;
    private Surface surface;
    
    // ==================== 构造方法 ====================
    
    public AVSDKPlayer() {
        this(Config.defaultConfig());
    }
    
    public AVSDKPlayer(Config config) {
        nativeHandle = nativeCreate(config);
    }
    
    // ==================== 回调设置 ====================
    
    public void setCallback(Callback callback) {
        this.callback = callback;
        nativeSetCallback(nativeHandle, callback);
    }
    
    // ==================== 数据源设置 ====================
    
    public void setDataSource(String url) throws AVSDKException {
        int result = nativeSetDataSource(nativeHandle, url);
        if (result != 0) {
            throw new AVSDKException(ErrorCode.values()[result], "Failed to set data source");
        }
    }
    
    public void setDataSource(Context context, android.net.Uri uri) throws AVSDKException {
        setDataSource(uri.toString());
    }
    
    // ==================== 渲染设置 ====================
    
    public void setSurface(Surface surface) {
        this.surface = surface;
        nativeSetSurface(nativeHandle, surface);
    }
    
    public void setSurfaceView(SurfaceView surfaceView) {
        setSurface(surfaceView.getHolder().getSurface());
    }
    
    public void setTextureView(TextureView textureView) {
        setSurface(new Surface(textureView.getSurfaceTexture()));
    }
    
    // ==================== 播放控制 ====================
    
    public void prepare() {
        nativePrepare(nativeHandle);
    }
    
    public void prepareSync() throws AVSDKException {
        int result = nativePrepareSync(nativeHandle);
        if (result != 0) {
            throw new AVSDKException(ErrorCode.values()[result], "Prepare failed");
        }
    }
    
    public void start() throws AVSDKException {
        int result = nativeStart(nativeHandle);
        if (result != 0) {
            throw new AVSDKException(ErrorCode.values()[result], "Start failed");
        }
    }
    
    public void pause() throws AVSDKException {
        int result = nativePause(nativeHandle);
        if (result != 0) {
            throw new AVSDKException(ErrorCode.values()[result], "Pause failed");
        }
    }
    
    public void resume() throws AVSDKException {
        int result = nativeResume(nativeHandle);
        if (result != 0) {
            throw new AVSDKException(ErrorCode.values()[result], "Resume failed");
        }
    }
    
    public void stop() throws AVSDKException {
        int result = nativeStop(nativeHandle);
        if (result != 0) {
            throw new AVSDKException(ErrorCode.values()[result], "Stop failed");
        }
    }
    
    public void reset() throws AVSDKException {
        int result = nativeReset(nativeHandle);
        if (result != 0) {
            throw new AVSDKException(ErrorCode.values()[result], "Reset failed");
        }
    }
    
    // ==================== 跳转控制 ====================
    
    public void seekTo(long positionMs) {
        nativeSeekTo(nativeHandle, positionMs);
    }
    
    public void seekTo(long positionMs, boolean accurate) {
        nativeSeekTo(nativeHandle, positionMs, accurate);
    }
    
    // ==================== 播放参数 ====================
    
    public void setPlaybackSpeed(float speed) {
        nativeSetPlaybackSpeed(nativeHandle, speed);
    }
    
    public float getPlaybackSpeed() {
        return nativeGetPlaybackSpeed(nativeHandle);
    }
    
    public void setLooping(boolean looping) {
        nativeSetLooping(nativeHandle, looping);
    }
    
    public boolean isLooping() {
        return nativeIsLooping(nativeHandle);
    }
    
    // ==================== 状态查询 ====================
    
    public State getState() {
        return State.values()[nativeGetState(nativeHandle)];
    }
    
    public long getCurrentPosition() {
        return nativeGetCurrentPosition(nativeHandle);
    }
    
    public long getDuration() {
        return nativeGetDuration(nativeHandle);
    }
    
    public long getBufferedPosition() {
        return nativeGetBufferedPosition(nativeHandle);
    }
    
    public int getBufferPercentage() {
        return nativeGetBufferPercentage(nativeHandle);
    }
    
    public boolean isPlaying() {
        return nativeIsPlaying(nativeHandle);
    }
    
    public boolean isPrepared() {
        return nativeIsPrepared(nativeHandle);
    }
    
    // ==================== 媒体信息 ====================
    
    public MediaInfo getMediaInfo() {
        return nativeGetMediaInfo(nativeHandle);
    }
    
    public int getVideoWidth() {
        return nativeGetVideoWidth(nativeHandle);
    }
    
    public int getVideoHeight() {
        return nativeGetVideoHeight(nativeHandle);
    }
    
    // ==================== 音量控制 ====================
    
    public void setVolume(int volume) {
        nativeSetVolume(nativeHandle, volume);
    }
    
    public int getVolume() {
        return nativeGetVolume(nativeHandle);
    }
    
    public void setMute(boolean mute) {
        nativeSetMute(nativeHandle, mute);
    }
    
    public boolean isMute() {
        return nativeIsMute(nativeHandle);
    }
    
    // ==================== 视频控制 ====================
    
    public void setScaleMode(ScaleMode mode) {
        nativeSetScaleMode(nativeHandle, mode.getValue());
    }
    
    public ScaleMode getScaleMode() {
        return ScaleMode.values()[nativeGetScaleMode(nativeHandle)];
    }
    
    public Bitmap captureFrame() {
        return nativeCaptureFrame(nativeHandle);
    }
    
    // ==================== 数据旁路 ====================
    
    public void enableVideoFrameCallback(boolean enable) {
        nativeEnableVideoFrameCallback(nativeHandle, enable);
    }
    
    public void enableAudioFrameCallback(boolean enable) {
        nativeEnableAudioFrameCallback(nativeHandle, enable);
    }
    
    // ==================== 资源释放 ====================
    
    public void release() {
        if (nativeHandle != 0) {
            nativeRelease(nativeHandle);
            nativeHandle = 0;
        }
    }
    
    @Override
    protected void finalize() throws Throwable {
        try {
            release();
        } finally {
            super.finalize();
        }
    }
    
    // ==================== Native方法 ====================
    
    private native long nativeCreate(Config config);
    private native void nativeSetCallback(long handle, Callback callback);
    private native int nativeSetDataSource(long handle, String url);
    private native void nativeSetSurface(long handle, Surface surface);
    private native void nativePrepare(long handle);
    private native int nativePrepareSync(long handle);
    private native int nativeStart(long handle);
    private native int nativePause(long handle);
    private native int nativeResume(long handle);
    private native int nativeStop(long handle);
    private native int nativeReset(long handle);
    private native void nativeSeekTo(long handle, long positionMs);
    private native void nativeSeekTo(long handle, long positionMs, boolean accurate);
    private native void nativeSetPlaybackSpeed(long handle, float speed);
    private native float nativeGetPlaybackSpeed(long handle);
    private native void nativeSetLooping(long handle, boolean looping);
    private native boolean nativeIsLooping(long handle);
    private native int nativeGetState(long handle);
    private native long nativeGetCurrentPosition(long handle);
    private native long nativeGetDuration(long handle);
    private native long nativeGetBufferedPosition(long handle);
    private native int nativeGetBufferPercentage(long handle);
    private native boolean nativeIsPlaying(long handle);
    private native boolean nativeIsPrepared(long handle);
    private native MediaInfo nativeGetMediaInfo(long handle);
    private native int nativeGetVideoWidth(long handle);
    private native int nativeGetVideoHeight(long handle);
    private native void nativeSetVolume(long handle, int volume);
    private native int nativeGetVolume(long handle);
    private native void nativeSetMute(long handle, boolean mute);
    private native boolean nativeIsMute(long handle);
    private native void nativeSetScaleMode(long handle, int mode);
    private native int nativeGetScaleMode(long handle);
    private native Bitmap nativeCaptureFrame(long handle);
    private native void nativeEnableVideoFrameCallback(long handle, boolean enable);
    private native void nativeEnableAudioFrameCallback(long handle, boolean enable);
    private native void nativeRelease(long handle);
    
    // ==================== 全局方法 ====================
    
    public static String getVersion() {
        return nativeGetVersion();
    }
    
    public static void initialize(Context context) {
        nativeInitialize(context);
    }
    
    public static void terminate() {
        nativeTerminate();
    }
    
    private static native String nativeGetVersion();
    private static native void nativeInitialize(Context context);
    private static native void nativeTerminate();
}
```

### 4.2 JNI桥接层

```cpp
// 文件: jni/player_jni.cpp

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "avsdk/player.h"
#include "avsdk/player_config.h"

using namespace AVSDK;

// JNI回调桥接类
class JNICallbackBridge : public IPlayerCallback {
public:
    JNIEnv* env;
    jobject jcallback;
    JavaVM* vm;
    
    JNICallbackBridge(JavaVM* jvm, jobject callback) : vm(jvm), jcallback(callback) {}
    
    void OnStateChanged(PlayerState oldState, PlayerState newState) override {
        JNIEnv* env;
        vm->AttachCurrentThread(&env, nullptr);
        
        jclass callbackClass = env->GetObjectClass(jcallback);
        jmethodID methodId = env->GetMethodID(callbackClass, "onStateChanged", 
            "(Lcom/avsdk/player/AVSDKPlayer$State;Lcom/avsdk/player/AVSDKPlayer$State;)V");
        
        // 调用Java回调
        // ...
        
        vm->DetachCurrentThread();
    }
    
    void OnPrepared(const MediaInfo& info) override {
        // 桥接到Java
    }
    
    void OnCompletion() override {}
    void OnBufferingStart() override {}
    void OnBufferingEnd() override {}
    void OnBufferingUpdate(int percent) override {}
    void OnProgressUpdate(Timestamp currentMs, Timestamp durationMs) override {}
    void OnVideoSizeChanged(int width, int height) override {}
    void OnFirstFrameRendered() override {}
    void OnError(const ErrorInfo& error) override {}
    void OnWarning(const ErrorInfo& warning) override {}
    void OnMetadata(const std::string& key, const std::string& value) override {}
};

// Android视频渲染器
class AndroidVideoRenderer : public IVideoRenderer {
public:
    ANativeWindow* nativeWindow = nullptr;
    
    ErrorCode Initialize(void* nativeView) override {
        nativeWindow = static_cast<ANativeWindow*>(nativeView);
        return ErrorCode::OK;
    }
    
    void Release() override {
        if (nativeWindow) {
            ANativeWindow_release(nativeWindow);
            nativeWindow = nullptr;
        }
    }
    
    ErrorCode RenderFrame(const VideoFrame& frame) override {
        // 使用MediaCodec或OpenGL ES渲染
        // ...
        return ErrorCode::OK;
    }
    
    void Clear() override {}
    void SetScaleMode(ScaleMode mode) override {}
    void SetRenderRect(int x, int y, int width, int height) override {}
    ErrorCode CaptureFrame(std::vector<uint8_t>& data, int& width, int& height) override {
        return ErrorCode::NotImplemented;
    }
};

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_avsdk_player_AVSDKPlayer_nativeCreate(JNIEnv* env, jobject thiz, jobject jconfig) {
    // 解析配置
    PlayerConfig config;
    // ... 从jconfig读取配置
    
    auto player = PlayerFactory::CreatePlayer();
    player->Initialize(config);
    
    // 保存指针
    return reinterpret_cast<jlong>(new std::shared_ptr<IPlayer>(player));
}

JNIEXPORT void JNICALL
Java_com_avsdk_player_AVSDKPlayer_nativeSetSurface(JNIEnv* env, jobject thiz, 
                                                    jlong handle, jobject jsurface) {
    auto player = *reinterpret_cast<std::shared_ptr<IPlayer>*>(handle);
    
    // 获取ANativeWindow
    ANativeWindow* window = ANativeWindow_fromSurface(env, jsurface);
    
    // 创建Android渲染器
    auto renderer = std::make_shared<AndroidVideoRenderer>();
    renderer->Initialize(window);
    
    player->SetVideoRenderer(renderer);
}

JNIEXPORT jint JNICALL
Java_com_avsdk_player_AVSDKPlayer_nativeStart(JNIEnv* env, jobject thiz, jlong handle) {
    auto player = *reinterpret_cast<std::shared_ptr<IPlayer>*>(handle);
    return static_cast<jint>(player->Start());
}

JNIEXPORT void JNICALL
Java_com_avsdk_player_AVSDKPlayer_nativeRelease(JNIEnv* env, jobject thiz, jlong handle) {
    auto playerPtr = reinterpret_cast<std::shared_ptr<IPlayer>*>(handle);
    delete playerPtr;
}

} // extern "C"
```

### 4.3 Kotlin扩展

```kotlin
// 文件: AVSDKPlayerExt.kt

package com.avsdk.player

import android.net.Uri
import android.view.SurfaceView
import android.view.TextureView
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/**
 * 异步准备播放器
 */
suspend fun AVSDKPlayer.prepareAsync(): AVSDKPlayer.MediaInfo = 
    suspendCancellableCoroutine { continuation ->
        setCallback(object : AVSDKPlayer.Callback {
            override fun onPrepared(mediaInfo: AVSDKPlayer.MediaInfo) {
                continuation.resume(mediaInfo)
            }
            
            override fun onError(error: AVSDKPlayer.AVSDKError) {
                continuation.resumeWithException(AVSDKException(error))
            }
        })
        prepare()
    }

/**
 * 异步跳转
 */
suspend fun AVSDKPlayer.seekToAsync(positionMs: Long) {
    // 实现异步跳转
}

/**
 * 播放（挂起函数）
 */
suspend fun AVSDKPlayer.play() {
    start()
}

/**
 * 播放器扩展属性
 */
val AVSDKPlayer.progress: Float
    get() = if (duration > 0) currentPosition.toFloat() / duration else 0f

val AVSDKPlayer.bufferProgress: Float
    get() = if (duration > 0) bufferedPosition.toFloat() / duration else 0f

/**
 * 使用DSL创建配置
 */
fun playerConfig(block: AVSDKPlayer.Config.() -> Unit): AVSDKPlayer.Config {
    return AVSDKPlayer.Config().apply(block)
}

// 使用示例
// val config = playerConfig {
//     decodeMode = AVSDKPlayer.DecodeMode.HARDWARE_FIRST
//     bufferTimeMs = 1000
//     enableHardwareDecoder = true
// }
```

---

## 5. Windows平台接口

### 5.1 C++接口（直接使用核心）

Windows平台可以直接使用C++核心API，无需额外包装。

```cpp
// 文件: examples/windows_player.cpp

#include <windows.h>
#include <avsdk/player.h>
#include <avsdk/player_config.h>
#include <iostream>

using namespace AVSDK;

// Windows视频渲染器（使用Direct3D）
class D3DVideoRenderer : public IVideoRenderer {
    // Direct3D相关成员
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    // ...
    
public:
    ErrorCode Initialize(void* nativeView) override {
        HWND hwnd = static_cast<HWND>(nativeView);
        // 初始化Direct3D
        // ...
        return ErrorCode::OK;
    }
    
    void Release() override {
        // 释放Direct3D资源
    }
    
    ErrorCode RenderFrame(const VideoFrame& frame) override {
        // 使用Direct3D渲染
        // ...
        return ErrorCode::OK;
    }
    
    void Clear() override {}
    void SetScaleMode(ScaleMode mode) override {}
    void SetRenderRect(int x, int y, int width, int height) override {}
    ErrorCode CaptureFrame(std::vector<uint8_t>& data, int& width, int& height) override {
        return ErrorCode::NotImplemented;
    }
};

// Windows播放器示例
class WindowsPlayer {
    std::shared_ptr<IPlayer> player;
    std::shared_ptr<D3DVideoRenderer> renderer;
    
public:
    bool Initialize(HWND hwnd) {
        // 初始化SDK
        PlayerFactory::Initialize();
        
        // 创建播放器
        player = PlayerFactory::CreatePlayer();
        
        // 配置
        PlayerConfig config;
        config.decodeMode = DecodeMode::HardwareFirst;
        config.enableHardwareDecoder = true;
        config.bufferStrategy = BufferStrategy::Smooth;
        
        player->Initialize(config);
        
        // 创建渲染器
        renderer = std::make_shared<D3DVideoRenderer>();
        renderer->Initialize(hwnd);
        player->SetVideoRenderer(renderer);
        
        // 设置回调
        player->SetCallbacks({
            .onPrepared = [](const MediaInfo& info) {
                std::cout << "Prepared: " << info.duration << "ms" << std::endl;
            },
            .onError = [](const ErrorInfo& error) {
                std::cerr << "Error: " << error.message << std::endl;
            }
        });
        
        return true;
    }
    
    bool Open(const std::wstring& url) {
        std::string urlUtf8(url.begin(), url.end());
        
        if (player->SetDataSource(urlUtf8) != ErrorCode::OK) {
            return false;
        }
        
        player->Prepare();
        return true;
    }
    
    void Play() {
        player->Start();
    }
    
    void Pause() {
        player->Pause();
    }
    
    void Stop() {
        player->Stop();
    }
    
    void Seek(int64_t positionMs) {
        player->SeekTo(positionMs);
    }
    
    void Release() {
        player->Release();
        PlayerFactory::Terminate();
    }
};

// Win32窗口消息处理
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static WindowsPlayer* player = nullptr;
    
    switch (msg) {
        case WM_CREATE:
            player = new WindowsPlayer();
            player->Initialize(hwnd);
            break;
            
        case WM_DESTROY:
            if (player) {
                player->Release();
                delete player;
            }
            PostQuitMessage(0);
            break;
            
        case WM_SIZE:
            // 处理窗口大小变化
            break;
            
        case WM_KEYDOWN:
            switch (wParam) {
                case VK_SPACE:
                    // 播放/暂停
                    break;
                case VK_LEFT:
                    // 后退
                    break;
                case VK_RIGHT:
                    // 前进
                    break;
            }
            break;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    // 注册窗口类
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "AVSDKPlayerWindow";
    RegisterClassEx(&wc);
    
    // 创建窗口
    HWND hwnd = CreateWindowEx(0, "AVSDKPlayerWindow", "AVSDK Player",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720, nullptr, nullptr, hInstance, nullptr);
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
```

### 5.2 C#包装接口（可选）

```csharp
// 文件: AVSDKPlayer.cs

using System;
using System.Runtime.InteropServices;

namespace AVSDK
{
    /// <summary>
    /// 播放器状态
    /// </summary>
    public enum PlayerState
    {
        Idle = 0,
        Initializing,
        Ready,
        Buffering,
        Playing,
        Paused,
        Seeking,
        Stopped,
        Error,
        Released
    }
    
    /// <summary>
    /// 解码模式
    /// </summary>
    public enum DecodeMode
    {
        Auto = 0,
        Software,
        Hardware,
        HardwareFirst
    }
    
    /// <summary>
    /// 缓冲策略
    /// </summary>
    public enum BufferStrategy
    {
        LowLatency = 0,
        Smooth,
        Balanced,
        Custom
    }
    
    /// <summary>
    /// 缩放模式
    /// </summary>
    public enum ScaleMode
    {
        Fit = 0,
        Fill,
        Stretch,
        Center
    }
    
    /// <summary>
    /// 错误码
    /// </summary>
    public enum ErrorCode
    {
        OK = 0,
        Unknown = 1,
        InvalidParameter = 2,
        OutOfMemory = 4,
        PlayerOpenFailed = 103,
        NetworkConnectFailed = 300,
        FileNotFound = 400,
        HardwareNotAvailable = 500
    }
    
    /// <summary>
    /// AVSDK异常
    /// </summary>
    public class AVSDKException : Exception
    {
        public ErrorCode Code { get; }
        
        public AVSDKException(ErrorCode code, string message) : base(message)
        {
            Code = code;
        }
    }
    
    /// <summary>
    /// 播放器配置
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct PlayerConfig
    {
        public DecodeMode DecodeMode;
        [MarshalAs(UnmanagedType.U1)]
        public bool EnableHardwareDecoder;
        [MarshalAs(UnmanagedType.U1)]
        public bool EnableH265Decoder;
        public int MaxDecodeThreads;
        public BufferStrategy BufferStrategy;
        public int BufferTimeMs;
        public int MinBufferTimeMs;
        public int MaxBufferTimeMs;
        public int ConnectTimeoutMs;
        public int ReadTimeoutMs;
        public ScaleMode ScaleMode;
        [MarshalAs(UnmanagedType.U1)]
        public bool EnableHdr;
        public int Volume;
        [MarshalAs(UnmanagedType.U1)]
        public bool Mute;
    }
    
    /// <summary>
    /// 媒体信息
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct MediaInfo
    {
        public IntPtr Url;
        public IntPtr Format;
        public long FileSize;
        public long DurationMs;
        public double Bitrate;
        [MarshalAs(UnmanagedType.U1)]
        public bool HasVideo;
        public int VideoWidth;
        public int VideoHeight;
        public double VideoFrameRate;
        public IntPtr VideoCodec;
        [MarshalAs(UnmanagedType.U1)]
        public bool HasAudio;
        public int AudioSampleRate;
        public int AudioChannels;
        public IntPtr AudioCodec;
    }
    
    /// <summary>
    /// 播放器类
    /// </summary>
    public class Player : IDisposable
    {
        private IntPtr _handle;
        
        #region Native Methods
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr AVSDK_Player_Create(ref PlayerConfig config);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void AVSDK_Player_Release(IntPtr handle);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int AVSDK_Player_SetDataSource(IntPtr handle, [MarshalAs(UnmanagedType.LPStr)] string url);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int AVSDK_Player_Prepare(IntPtr handle);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int AVSDK_Player_Start(IntPtr handle);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int AVSDK_Player_Pause(IntPtr handle);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int AVSDK_Player_Stop(IntPtr handle);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void AVSDK_Player_SeekTo(IntPtr handle, long positionMs);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern PlayerState AVSDK_Player_GetState(IntPtr handle);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern long AVSDK_Player_GetCurrentPosition(IntPtr handle);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern long AVSDK_Player_GetDuration(IntPtr handle);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void AVSDK_Player_SetVolume(IntPtr handle, int volume);
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void AVSDK_Player_SetMute(IntPtr handle, [MarshalAs(UnmanagedType.U1)] bool mute);
        
        #endregion
        
        /// <summary>
        /// 构造函数
        /// </summary>
        public Player(PlayerConfig config)
        {
            _handle = AVSDK_Player_Create(ref config);
            if (_handle == IntPtr.Zero)
            {
                throw new AVSDKException(ErrorCode.Unknown, "Failed to create player");
            }
        }
        
        /// <summary>
        /// 设置数据源
        /// </summary>
        public void SetDataSource(string url)
        {
            int result = AVSDK_Player_SetDataSource(_handle, url);
            if (result != 0)
            {
                throw new AVSDKException((ErrorCode)result, "Failed to set data source");
            }
        }
        
        /// <summary>
        /// 准备播放
        /// </summary>
        public void Prepare()
        {
            int result = AVSDK_Player_Prepare(_handle);
            if (result != 0)
            {
                throw new AVSDKException((ErrorCode)result, "Failed to prepare");
            }
        }
        
        /// <summary>
        /// 开始播放
        /// </summary>
        public void Play()
        {
            int result = AVSDK_Player_Start(_handle);
            if (result != 0)
            {
                throw new AVSDKException((ErrorCode)result, "Failed to start");
            }
        }
        
        /// <summary>
        /// 暂停
        /// </summary>
        public void Pause()
        {
            int result = AVSDK_Player_Pause(_handle);
            if (result != 0)
            {
                throw new AVSDKException((ErrorCode)result, "Failed to pause");
            }
        }
        
        /// <summary>
        /// 停止
        /// </summary>
        public void Stop()
        {
            int result = AVSDK_Player_Stop(_handle);
            if (result != 0)
            {
                throw new AVSDKException((ErrorCode)result, "Failed to stop");
            }
        }
        
        /// <summary>
        /// 跳转
        /// </summary>
        public void SeekTo(TimeSpan position)
        {
            AVSDK_Player_SeekTo(_handle, (long)position.TotalMilliseconds);
        }
        
        /// <summary>
        /// 当前状态
        /// </summary>
        public PlayerState State => AVSDK_Player_GetState(_handle);
        
        /// <summary>
        /// 当前位置
        /// </summary>
        public TimeSpan CurrentPosition => TimeSpan.FromMilliseconds(AVSDK_Player_GetCurrentPosition(_handle));
        
        /// <summary>
        /// 总时长
        /// </summary>
        public TimeSpan Duration => TimeSpan.FromMilliseconds(AVSDK_Player_GetDuration(_handle));
        
        /// <summary>
        /// 音量 (0-100)
        /// </summary>
        public int Volume
        {
            set => AVSDK_Player_SetVolume(_handle, value);
        }
        
        /// <summary>
        /// 静音
        /// </summary>
        public bool Mute
        {
            set => AVSDK_Player_SetMute(_handle, value);
        }
        
        /// <summary>
        /// 释放资源
        /// </summary>
        public void Dispose()
        {
            if (_handle != IntPtr.Zero)
            {
                AVSDK_Player_Release(_handle);
                _handle = IntPtr.Zero;
            }
        }
    }
    
    /// <summary>
    /// SDK全局方法
    /// </summary>
    public static class AVSDK
    {
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr AVSDK_GetVersion();
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int AVSDK_Initialize();
        
        [DllImport("avsdk.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void AVSDK_Terminate();
        
        /// <summary>
        /// 版本号
        /// </summary>
        public static string Version
        {
            get
            {
                IntPtr ptr = AVSDK_GetVersion();
                return Marshal.PtrToStringAnsi(ptr);
            }
        }
        
        /// <summary>
        /// 初始化SDK
        /// </summary>
        public static void Initialize()
        {
            int result = AVSDK_Initialize();
            if (result != 0)
            {
                throw new AVSDKException((ErrorCode)result, "Failed to initialize SDK");
            }
        }
        
        /// <summary>
        /// 终止SDK
        /// </summary>
        public static void Terminate()
        {
            AVSDK_Terminate();
        }
    }
}
```

---

## 6. 错误码定义

### 6.1 完整错误码表

| 错误码 | 名称 | 说明 | 处理建议 |
|--------|------|------|----------|
| 0 | OK | 成功 | - |
| 1 | Unknown | 未知错误 | 查看日志，联系技术支持 |
| 2 | InvalidParameter | 无效参数 | 检查传入参数 |
| 3 | NullPointer | 空指针 | 检查对象是否已初始化 |
| 4 | OutOfMemory | 内存不足 | 释放资源，降低分辨率 |
| 5 | NotImplemented | 未实现 | 使用其他功能替代 |
| 6 | NotInitialized | 未初始化 | 先调用Initialize |
| 7 | AlreadyInitialized | 已初始化 | 无需重复初始化 |
| 8 | OperationNotAllowed | 操作不允许 | 检查当前状态 |
| 9 | Timeout | 超时 | 检查网络，增加超时时间 |
| 10 | Cancelled | 已取消 | 用户主动取消 |
| 100 | PlayerNotCreated | 播放器未创建 | 先创建播放器 |
| 101 | PlayerAlreadyPlaying | 已在播放 | 无需重复开始 |
| 102 | PlayerNotPlaying | 未在播放 | 先调用Start |
| 103 | PlayerOpenFailed | 打开媒体失败 | 检查URL，网络连接 |
| 104 | PlayerSeekFailed | 跳转失败 | 检查时间范围 |
| 105 | PlayerNoVideoStream | 无视频流 | 检查媒体文件 |
| 106 | PlayerNoAudioStream | 无音频流 | 检查媒体文件 |
| 107 | PlayerRenderFailed | 渲染失败 | 检查渲染器设置 |
| 108 | PlayerBufferFull | 缓冲区满 | 降低码率或分辨率 |
| 109 | PlayerBufferEmpty | 缓冲区空 | 检查网络连接 |
| 110 | PlayerEndOfStream | 流结束 | 正常结束 |
| 200 | CodecNotFound | 编解码器未找到 | 检查FFmpeg编译配置 |
| 201 | CodecOpenFailed | 打开编解码器失败 | 检查编解码器支持 |
| 202 | CodecDecodeFailed | 解码失败 | 检查媒体文件完整性 |
| 203 | CodecEncodeFailed | 编码失败 | 检查编码参数 |
| 204 | CodecNotSupported | 不支持的编解码器 | 使用其他格式 |
| 205 | CodecHardwareFailed | 硬件编解码失败 | 回退到软件编解码 |
| 206 | CodecSoftwareFallback | 回退到软件编解码 | 警告，可忽略 |
| 300 | NetworkConnectFailed | 连接失败 | 检查网络，URL |
| 301 | NetworkDisconnected | 连接断开 | 检查网络稳定性 |
| 302 | NetworkTimeout | 网络超时 | 增加超时时间 |
| 303 | NetworkResolveFailed | 解析域名失败 | 检查DNS设置 |
| 304 | NetworkInvalidURL | 无效URL | 检查URL格式 |
| 305 | NetworkIOError | 网络IO错误 | 检查网络连接 |
| 306 | NetworkHttpError | HTTP错误 | 检查服务器状态 |
| 400 | FileNotFound | 文件不存在 | 检查文件路径 |
| 401 | FileOpenFailed | 打开文件失败 | 检查文件权限 |
| 402 | FileReadFailed | 读取文件失败 | 检查文件完整性 |
| 403 | FileWriteFailed | 写入文件失败 | 检查磁盘空间 |
| 404 | FileInvalidFormat | 无效文件格式 | 检查文件格式 |
| 405 | FileCorrupted | 文件损坏 | 尝试修复或重新获取 |
| 500 | HardwareNotAvailable | 硬件不可用 | 使用软件编解码 |
| 501 | HardwareInitFailed | 硬件初始化失败 | 重启应用或设备 |
| 502 | HardwareSurfaceFailed | 硬件表面创建失败 | 检查GPU驱动 |
| 503 | HardwareContextFailed | 硬件上下文失败 | 重启应用或设备 |
| 600 | DRMNotSupported | DRM不支持 | 使用非DRM内容 |
| 601 | DRMLicenseFailed | 许可证获取失败 | 检查许可证服务 |
| 602 | DRMDecryptFailed | 解密失败 | 检查许可证有效性 |

---

## 7. 配置参数详解

### 7.1 播放配置参数

```cpp
struct PlayerConfig {
    // ==================== 解码配置 ====================
    
    // 解码模式
    // - Auto: 自动选择（默认）
    // - Software: 强制软件解码
    // - Hardware: 强制硬件解码
    // - HardwareFirst: 优先硬件，失败回退软件
    DecodeMode decodeMode = DecodeMode::Auto;
    
    // 启用硬件解码器
    // 影响: 性能、功耗、兼容性
    // 建议: 默认开启，遇到问题时关闭
    bool enableHardwareDecoder = true;
    
    // 启用H265解码
    // 影响: 支持H265/HEVC格式
    // 建议: 默认开启
    bool enableH265Decoder = true;
    
    // 启用多线程解码
    // 影响: CPU使用率、解码延迟
    // 建议: 开启，设置2-4线程
    bool enableMultiThreadDecode = true;
    
    // 最大解码线程数
    // 范围: 1-8
    // 建议: 4（1080p），8（4K）
    int maxDecodeThreads = 4;
    
    // ==================== 缓冲配置 ====================
    
    // 缓冲策略
    // - LowLatency: 低延迟（直播）
    // - Smooth: 流畅优先（点播）
    // - Balanced: 平衡（默认）
    // - Custom: 自定义
    BufferStrategy bufferStrategy = BufferStrategy::Balanced;
    
    // 缓冲时间（毫秒）
    // 影响: 启动延迟、卡顿频率
    // 建议: 直播300-500ms，点播1000-3000ms
    int bufferTimeMs = 1000;
    
    // 最小缓冲时间
    // 影响: 恢复播放的时机
    // 建议: bufferTimeMs的一半
    int minBufferTimeMs = 500;
    
    // 最大缓冲时间
    // 影响: 内存使用
    // 建议: 点播5000-10000ms
    int maxBufferTimeMs = 5000;
    
    // 最大缓冲区大小（字节）
    // 影响: 内存使用
    // 建议: 50-100MB
    int maxBufferSize = 50 * 1024 * 1024;
    
    // ==================== 网络配置 ====================
    
    // 连接超时（毫秒）
    // 影响: 连接失败的判定时间
    // 建议: 5000-10000ms
    int connectTimeoutMs = 10000;
    
    // 读取超时（毫秒）
    // 影响: 卡顿的判定时间
    // 建议: 10000-30000ms
    int readTimeoutMs = 30000;
    
    // 重试次数
    // 影响: 网络波动时的恢复能力
    // 建议: 3-5次
    int retryCount = 3;
    
    // 重试间隔（毫秒）
    // 影响: 重试频率
    // 建议: 1000-3000ms
    int retryIntervalMs = 1000;
    
    // HTTP Keep-Alive
    // 影响: 连接复用
    // 建议: 开启
    bool enableHttpKeepAlive = true;
    
    // User-Agent
    // 影响: 服务器识别
    // 建议: 设置应用标识
    std::string userAgent = "AVSDK/1.0.0";
    
    // 自定义HTTP头
    // 用途: 认证、特殊配置
    std::vector<std::string> headers;
    
    // ==================== 视频配置 ====================
    
    // 缩放模式
    // - Fit: 适应（保持比例）
    // - Fill: 填充（保持比例）
    // - Stretch: 拉伸
    // - Center: 居中
    ScaleMode scaleMode = ScaleMode::Fit;
    
    // 去隔行
    // 影响: 交错视频的显示质量
    // 建议: 仅在需要时开启
    bool enableDeinterlace = false;
    
    // HDR支持
    // 影响: HDR视频的显示
    // 建议: 默认开启
    bool enableHdr = true;
    
    // ==================== 音频配置 ====================
    
    // 音量 (0-100)
    int audioVolume = 100;
    
    // 静音
    bool audioMute = false;
    
    // 音频延迟调整（毫秒）
    // 用途: 音画同步
    int audioLatencyMs = 0;
    
    // ==================== 调试配置 ====================
    
    // 启用日志
    bool enableLog = false;
    
    // 日志级别 (0-5)
    // 0: Verbose, 1: Debug, 2: Info, 3: Warning, 4: Error, 5: Fatal
    int logLevel = 2;
    
    // 日志文件路径
    std::string logPath;
};
```

### 7.2 编码配置参数

```cpp
struct EncoderConfig {
    // ==================== 视频编码配置 ====================
    
    // 视频编码器类型
    // - Auto: 自动选择
    // - H264: H.264/AVC
    // - H265: H.265/HEVC
    // - VP8: VP8
    // - VP9: VP9
    // - AV1: AV1
    VideoEncoderType videoEncoderType = VideoEncoderType::H264;
    
    // 编码器实现
    // - Auto: 自动选择
    // - Software: 软件编码
    // - Hardware: 硬件编码
    EncoderImplementation encoderImpl = EncoderImplementation::Auto;
    
    // 分辨率
    // 常见: 1920x1080, 1280x720, 854x480
    VideoResolution resolution = {1920, 1080};
    
    // 帧率
    // 常见: 30, 60, 24
    int frameRate = 30;
    
    // 视频码率（bps）
    // 建议: 1080p 4Mbps, 720p 2Mbps, 480p 1Mbps
    int videoBitrate = 4000000;
    
    // 码率控制模式
    // - CBR: 恒定码率（直播推荐）
    // - VBR: 可变码率（点播推荐）
    // - CQ: 恒定质量
    // - CRF: 恒定速率因子
    BitrateControlMode bitrateMode = BitrateControlMode::VBR;
    
    // 视频质量（CRF）
    // 范围: 0-51，越小越好
    // 建议: 23（默认），18（高质量），28（低质量）
    int videoQuality = 23;
    
    // 关键帧间隔（秒）
    // 影响: 拖动响应、压缩效率
    // 建议: 2-4秒
    int keyFrameInterval = 2;
    
    // B帧数量
    // 影响: 压缩效率、延迟
    // 建议: 直播0-2，点播3-5
    int bFrames = 3;
    
    // 启用硬件加速
    // 影响: 编码速度、功耗
    // 建议: 默认开启
    bool enableHwAcceleration = true;
    
    // ==================== 音频编码配置 ====================
    
    // 启用音频编码
    bool enableAudio = true;
    
    // 音频采样率
    // 常见: 48000, 44100
    int audioSampleRate = 48000;
    
    // 音频通道数
    // 常见: 2（立体声）, 1（单声道）
    int audioChannels = 2;
    
    // 音频码率（bps）
    // 建议: 128000（128kbps）
    int audioBitrate = 128000;
    
    // 音频编码器
    // 常见: aac, opus, mp3
    std::string audioCodec = "aac";
    
    // ==================== 性能配置 ====================
    
    // 编码线程数
    // 建议: 4-8
    int encoderThreads = 4;
    
    // 编码预设（速度/质量权衡）
    // 0: ultrafast, 1: superfast, 2: veryfast, 3: faster, 4: fast
    // 5: medium, 6: slow, 7: slower, 8: veryslow, 9: placebo
    // 建议: 直播0-3，点播5-7
    int encoderPreset = 3;
};
```

---

## 8. 使用示例

### 8.1 C++核心使用示例

```cpp
#include <avsdk/player.h>
#include <avsdk/player_config.h>
#include <iostream>

using namespace AVSDK;

// 自定义回调
class MyPlayerCallback : public IPlayerCallback {
public:
    void OnStateChanged(PlayerState oldState, PlayerState newState) override {
        std::cout << "State: " << (int)oldState << " -> " << (int)newState << std::endl;
    }
    
    void OnPrepared(const MediaInfo& info) override {
        std::cout << "Prepared: " << info.duration << "ms" << std::endl;
    }
    
    void OnCompletion() override {
        std::cout << "Playback completed" << std::endl;
    }
    
    void OnBufferingStart() override {
        std::cout << "Buffering started" << std::endl;
    }
    
    void OnBufferingEnd() override {
        std::cout << "Buffering ended" << std::endl;
    }
    
    void OnBufferingUpdate(int percent) override {
        std::cout << "Buffering: " << percent << "%" << std::endl;
    }
    
    void OnProgressUpdate(Timestamp currentMs, Timestamp durationMs) override {
        std::cout << "Progress: " << currentMs << "/" << durationMs << std::endl;
    }
    
    void OnVideoSizeChanged(int width, int height) override {
        std::cout << "Video size: " << width << "x" << height << std::endl;
    }
    
    void OnFirstFrameRendered() override {
        std::cout << "First frame rendered" << std::endl;
    }
    
    void OnError(const ErrorInfo& error) override {
        std::cerr << "Error: " << error.message << std::endl;
    }
    
    void OnWarning(const ErrorInfo& warning) override {
        std::cerr << "Warning: " << warning.message << std::endl;
    }
    
    void OnMetadata(const std::string& key, const std::string& value) override {
        std::cout << "Metadata: " << key << " = " << value << std::endl;
    }
};

int main() {
    // 初始化SDK
    PlayerFactory::Initialize();
    
    // 创建播放器
    auto player = PlayerFactory::CreatePlayer();
    
    // 配置
    PlayerConfig config;
    config.decodeMode = DecodeMode::HardwareFirst;
    config.enableHardwareDecoder = true;
    config.enableH265Decoder = true;
    config.bufferStrategy = BufferStrategy::Smooth;
    config.bufferTimeMs = 2000;
    config.scaleMode = ScaleMode::Fit;
    config.enableLog = true;
    
    player->Initialize(config);
    
    // 设置回调
    MyPlayerCallback callback;
    player->SetCallback(&callback);
    
    // 设置数据源
    player->SetDataSource("https://example.com/video.mp4");
    
    // 准备并播放
    player->Prepare();
    // 等待OnPrepared回调后
    player->Start();
    
    // 主循环
    // ...
    
    // 清理
    player->Release();
    PlayerFactory::Terminate();
    
    return 0;
}
```

### 8.2 iOS使用示例

```swift
import UIKit
import AVSDK

class PlayerViewController: UIViewController {
    
    private var player: AVSDKPlayer!
    private var playerView: UIView!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        // 初始化SDK
        AVSDKPlayer.initializeSDK(nil)
        
        // 创建播放器视图
        playerView = UIView(frame: view.bounds)
        playerView.backgroundColor = .black
        view.addSubview(playerView)
        
        // 创建配置
        let config = AVSDKPlayerConfig()
        config.decodeMode = .hardwareFirst
        config.bufferStrategy = .smooth
        config.enableHardwareDecoder = true
        config.enableH265Decoder = true
        
        // 创建播放器
        player = AVSDKPlayer(config: config)
        player.delegate = self
        
        // 绑定渲染视图
        player.attach(to: playerView)
        
        // 设置数据源
        let url = "https://example.com/video.mp4"
        try? player.setDataSource(url)
        
        // 准备并播放
        player.prepare()
    }
    
    // 播放控制
    @IBAction func playPauseTapped(_ sender: UIButton) {
        if player.isPlaying {
            try? player.pause(nil)
        } else {
            try? player.resume(nil)
        }
    }
    
    @IBAction func seekTapped(_ sender: UISlider) {
        let time = TimeInterval(sender.value) * player.duration
        player.seek(to: time)
    }
    
    // 截图
    @IBAction func captureTapped(_ sender: UIButton) {
        if let image = player.captureFrame() {
            UIImageWriteToSavedPhotosAlbum(image, nil, nil, nil)
        }
    }
    
    deinit {
        player.detachFromView()
        AVSDKPlayer.terminateSDK()
    }
}

// MARK: - AVSDKPlayerDelegate

extension PlayerViewController: AVSDKPlayerDelegate {
    
    func player(_ player: AVSDKPlayer, didChangeState oldState: AVSDKPlayerState, toState newState: AVSDKPlayerState) {
        print("State changed: \(oldState) -> \(newState)")
    }
    
    func player(_ player: AVSDKPlayer, didPrepareWithMediaInfo mediaInfo: AVSDKMediaInfo) {
        print("Prepared: \(mediaInfo.duration)s")
        try? player.start(nil)
    }
    
    func playerDidComplete(_ player: AVSDKPlayer) {
        print("Playback completed")
    }
    
    func player(_ player: AVSDKPlayer, didUpdateProgress currentTime: TimeInterval, duration: TimeInterval) {
        // 更新进度条
    }
    
    func player(_ player: AVSDKPlayer, didEncounterError error: AVSDKError) {
        print("Error: \(error.message)")
    }
}
```

### 8.3 Android使用示例

```kotlin
package com.example.avsdkdemo

import android.os.Bundle
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import com.avsdk.player.AVSDKPlayer
import com.avsdk.player.playerConfig
import kotlinx.coroutines.*

class PlayerActivity : AppCompatActivity() {
    
    private lateinit var player: AVSDKPlayer
    private lateinit var surfaceView: SurfaceView
    private lateinit var seekBar: SeekBar
    
    private val scope = CoroutineScope(Dispatchers.Main + Job())
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_player)
        
        // 初始化SDK
        AVSDKPlayer.initialize(this)
        
        // 初始化视图
        surfaceView = findViewById(R.id.surfaceView)
        seekBar = findViewById(R.id.seekBar)
        
        // 创建配置
        val config = playerConfig {
            decodeMode = AVSDKPlayer.DecodeMode.HARDWARE_FIRST
            bufferStrategy = AVSDKPlayer.BufferStrategy.SMOOTH
            enableHardwareDecoder = true
            enableH265Decoder = true
        }
        
        // 创建播放器
        player = AVSDKPlayer(config)
        
        // 设置Surface
        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                player.setSurface(holder.surface)
            }
            override fun surfaceChanged(holder: SurfaceHolder, format: Int, w: Int, h: Int) {}
            override fun surfaceDestroyed(holder: SurfaceHolder) {}
        })
        
        // 设置回调
        player.setCallback(object : AVSDKPlayer.Callback {
            override fun onPrepared(mediaInfo: AVSDKPlayer.MediaInfo) {
                runOnUiThread {
                    seekBar.max = mediaInfo.durationMs.toInt()
                }
                player.start()
            }
            
            override fun onProgressUpdate(currentMs: Long, durationMs: Long) {
                runOnUiThread {
                    seekBar.progress = currentMs.toInt()
                }
            }
            
            override fun onError(error: AVSDKPlayer.AVSDKError) {
                runOnUiThread {
                    Toast.makeText(this@PlayerActivity, 
                        "Error: ${error.message}", Toast.LENGTH_LONG).show()
                }
            }
        })
        
        // 加载视频
        scope.launch {
            try {
                player.setDataSource("https://example.com/video.mp4")
                player.prepareAsync()
            } catch (e: AVSDKException) {
                Toast.makeText(this@PlayerActivity, 
                    "Failed: ${e.message}", Toast.LENGTH_LONG).show()
            }
        }
        
        // 进度条控制
        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    player.seekTo(progress.toLong())
                }
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
    }
    
    override fun onPause() {
        super.onPause()
        player.pause()
    }
    
    override fun onResume() {
        super.onResume()
        player.resume()
    }
    
    override fun onDestroy() {
        super.onDestroy()
        scope.cancel()
        player.release()
        AVSDKPlayer.terminate()
    }
}
```

---

## 9. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0.0 | 2024-XX-XX | 初始版本，支持本地播放、网络流播放、实时编码、数据旁路 |

---

## 10. 附录

### 10.1 头文件目录结构

```
include/
├── avsdk/
│   ├── types.h           # 基础类型定义
│   ├── error.h           # 错误码定义
│   ├── player_config.h   # 播放器配置
│   ├── player_state.h    # 播放器状态
│   ├── player_callback.h # 播放器回调
│   ├── player.h          # 播放器核心
│   ├── encoder_config.h  # 编码器配置
│   ├── encoder.h         # 编码器核心
│   └── utils.h           # 工具类
└── avsdk.h               # 主头文件（包含所有）
```

### 10.2 平台实现目录结构

```
platform/
├── ios/
│   ├── AVSDKPlayer.h     # Objective-C头文件
│   ├── AVSDKPlayer.mm    # Objective-C++实现
│   ├── AVSDKPlayer+Swift.swift  # Swift扩展
│   └── VideoRenderer.mm  # iOS视频渲染器
├── android/
│   ├── AVSDKPlayer.java  # Java接口
│   ├── AVSDKPlayerExt.kt # Kotlin扩展
│   ├── jni/
│   │   └── player_jni.cpp    # JNI桥接
│   └── VideoRenderer.cpp # Android视频渲染器
├── macos/
│   └── VideoRenderer.mm  # macOS视频渲染器
└── windows/
    ├── AVSDKPlayer.cs    # C#包装
    └── VideoRenderer.cpp # D3D视频渲染器
```

---

*文档结束*
