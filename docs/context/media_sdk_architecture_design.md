# 跨平台音视频SDK架构设计文档

## 版本信息
- 版本: v1.0
- 日期: 2024年
- 作者: 音视频架构团队

---

## 1. 整体架构设计

### 1.1 分层架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              应用层 (Application Layer)                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │  iOS App    │  │ Android App │  │  macOS App  ││ Windows App │         │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘         │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          平台适配层 (Platform Adapter Layer)                  │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    平台抽象接口 (Platform Abstraction Interface)      │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │ 渲染抽象  │  │ 编解码抽象 │  │ 网络抽象  │  │ 采集抽象  │            │   │
│  │  │ Renderer │  │  Codec   │  │ Network  │  │ Capture  │            │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    平台具体实现 (Platform Implementation)             │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │iOS实现   │  │Android   │  │macOS实现 │  │Windows   │            │   │
│  │  │(Metal/VT)│  │(GLES/MC) │  │(Metal/VT)│  │(DX/DXVA) │            │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            核心层 (Core Layer)                               │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      媒体处理引擎 (Media Processing Engine)            │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │ Demuxer  │  │ Decoder  │  │ Encoder  │  │  Muxer   │            │   │
│  │  │ 解封装   │  │  解码    │  │  编码    │  │  封装    │            │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │  Renderer│  │ Resampler│  │  Filter  │  │  Clock   │            │   │
│  │  │  渲染    │  │ 重采样   │  │  滤镜    │  │  同步    │            │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      FFmpeg集成层 (FFmpeg Integration)                │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │ libavformat│ │ libavcodec│ │ libswscale│ │ libswresample│         │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          基础设施层 (Infrastructure Layer)                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐     │
│  │ 线程池   │  │ 内存池   │  │ 日志系统 │  │ 配置管理 │  │ 事件系统 │     │
│  │ThreadPool│  │MemoryPool│  │  Logger  │  │  Config  │  │  Event   │     │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  └──────────┘     │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 各层职责说明

| 层级 | 职责 | 关键技术 |
|------|------|----------|
| **应用层** | 提供各平台的UI应用，调用SDK接口 | Swift/Kotlin/Objective-C/C# |
| **平台适配层** | 封装平台特定API，提供统一接口 | Metal/OpenGL ES/DirectX/VideoToolbox/MediaCodec/DXVA |
| **核心层** | 实现媒体处理逻辑，与FFmpeg交互 | C++ / FFmpeg |
| **基础设施层** | 提供通用工具和服务 | C++ |

### 1.3 模块依赖关系

```
                    ┌─────────────┐
                    │   应用层     │
                    └──────┬──────┘
                           │ 依赖
                           ▼
              ┌────────────────────────┐
              │      平台适配层         │
              │  ┌──────────────────┐  │
              │  │   平台抽象接口    │  │
              │  └────────┬─────────┘  │
              │           │ 依赖        │
              │           ▼            │
              │  ┌──────────────────┐  │
              │  │   平台具体实现    │  │
              │  └──────────────────┘  │
              └───────────┬────────────┘
                          │ 依赖
                          ▼
              ┌────────────────────────┐
              │        核心层          │
              │  ┌──────────────────┐  │
              │  │   媒体处理引擎    │  │
              │  └────────┬─────────┘  │
              │           │ 依赖        │
              │           ▼            │
              │  ┌──────────────────┐  │
              │  │   FFmpeg集成层    │  │
              │  └──────────────────┘  │
              └───────────┬────────────┘
                          │ 依赖
                          ▼
              ┌────────────────────────┐
              │      基础设施层         │
              └────────────────────────┘
```

---

## 2. 模块划分

### 2.1 核心模块详细设计

#### 2.1.1 Demuxer（解封装模块）

```cpp
// 解封装模块接口
class IDemuxer {
public:
    virtual ~IDemuxer() = default;
    
    // 打开媒体源
    virtual int Open(const std::string& url) = 0;
    
    // 关闭媒体源
    virtual void Close() = 0;
    
    // 读取数据包
    virtual int ReadPacket(AVPacket* packet) = 0;
    
    // 定位到指定时间
    virtual int Seek(int64_t timestamp) = 0;
    
    // 获取媒体信息
    virtual const MediaInfo& GetMediaInfo() const = 0;
    
    // 获取视频流索引
    virtual int GetVideoStreamIndex() const = 0;
    
    // 获取音频流索引
    virtual int GetAudioStreamIndex() const = 0;
};

// 解封装模块实现
class FFmpegDemuxer : public IDemuxer {
private:
    AVFormatContext* format_ctx_ = nullptr;
    MediaInfo media_info_;
    int video_stream_index_ = -1;
    int audio_stream_index_ = -1;
    std::mutex mutex_;
};
```

#### 2.1.2 Decoder（解码模块）

```cpp
// 解码器类型枚举
enum class DecoderType {
    kSoftware,      // 软解码
    kHardware,      // 硬解码
    kAuto           // 自动选择
};

// 解码模块接口
class IDecoder {
public:
    virtual ~IDecoder() = default;
    
    // 初始化解码器
    virtual int Initialize(const AVCodecParameters* params, DecoderType type) = 0;
    
    // 发送数据包
    virtual int SendPacket(const AVPacket* packet) = 0;
    
    // 接收解码帧
    virtual int ReceiveFrame(AVFrame* frame) = 0;
    
    // 刷新解码器
    virtual void Flush() = 0;
    
    // 关闭解码器
    virtual void Close() = 0;
    
    // 获取解码器名称
    virtual std::string GetName() const = 0;
};

// 软件解码器实现
class FFmpegDecoder : public IDecoder {
private:
    AVCodecContext* codec_ctx_ = nullptr;
    const AVCodec* codec_ = nullptr;
    DecoderType type_;
};

// 硬件解码器抽象基类
class HardwareDecoder : public IDecoder {
protected:
    AVBufferRef* hw_device_ctx_ = nullptr;
    AVPixelFormat hw_pixel_format_ = AV_PIX_FMT_NONE;
    AVPixelFormat sw_pixel_format_ = AV_PIX_FMT_NONE;
};
```

#### 2.1.3 Encoder（编码模块）

```cpp
// 编码器配置
struct EncoderConfig {
    AVCodecID codec_id = AV_CODEC_ID_H264;
    int width = 1920;
    int height = 1080;
    int bitrate = 5000000;          // bps
    int fps = 30;
    int gop_size = 30;
    int keyint_min = 30;
    bool use_hardware = false;
    AVPixelFormat pixel_format = AV_PIX_FMT_YUV420P;
};

// 编码模块接口
class IEncoder {
public:
    virtual ~IEncoder() = default;
    
    // 初始化解码器
    virtual int Initialize(const EncoderConfig& config) = 0;
    
    // 发送原始帧
    virtual int SendFrame(const AVFrame* frame) = 0;
    
    // 接收编码包
    virtual int ReceivePacket(AVPacket* packet) = 0;
    
    // 刷新编码器
    virtual void Flush() = 0;
    
    // 关闭编码器
    virtual void Close() = 0;
    
    // 设置码率
    virtual int SetBitrate(int bitrate) = 0;
};

// H.265编码器实现
class H265Encoder : public IEncoder {
private:
    AVCodecContext* codec_ctx_ = nullptr;
    EncoderConfig config_;
    std::mutex mutex_;
};
```

#### 2.1.4 Muxer（封装模块）

```cpp
// 封装模块接口
class IMuxer {
public:
    virtual ~IMuxer() = default;
    
    // 创建输出文件
    virtual int Create(const std::string& url, const MuxerConfig& config) = 0;
    
    // 添加视频流
    virtual int AddVideoStream(const AVCodecParameters* params) = 0;
    
    // 添加音频流
    virtual int AddAudioStream(const AVCodecParameters* params) = 0;
    
    // 写入文件头
    virtual int WriteHeader() = 0;
    
    // 写入数据包
    virtual int WritePacket(const AVPacket* packet) = 0;
    
    // 写入文件尾
    virtual int WriteTrailer() = 0;
    
    // 关闭文件
    virtual void Close() = 0;
};
```

#### 2.1.5 Renderer（渲染模块）

```cpp
// 渲染模式
enum class RenderMode {
    kScaleAspectFit,      // 等比缩放适应
    kScaleAspectFill,     // 等比缩放填充
    kScaleToFill          // 拉伸填充
};

// 渲染模块接口（平台抽象）
class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    // 初始化渲染器
    virtual int Initialize(void* native_window, int width, int height) = 0;
    
    // 渲染视频帧
    virtual int RenderFrame(const AVFrame* frame) = 0;
    
    // 设置渲染区域
    virtual void SetRenderRect(int x, int y, int width, int height) = 0;
    
    // 设置渲染模式
    virtual void SetRenderMode(RenderMode mode) = 0;
    
    // 清屏
    virtual void Clear() = 0;
    
    // 释放资源
    virtual void Release() = 0;
};
```

### 2.2 平台抽象层模块

#### 2.2.1 渲染抽象层

```cpp
// 渲染后端类型
enum class RenderBackend {
    kOpenGLES,      // OpenGL ES (Android/iOS)
    kMetal,         // Metal (iOS/macOS)
    kDirectX,       // DirectX (Windows)
    kVulkan,        // Vulkan (跨平台)
    kSoftware       // 软件渲染
};

// 渲染抽象工厂
class IRenderFactory {
public:
    virtual ~IRenderFactory() = default;
    
    // 创建渲染器
    virtual std::unique_ptr<IRenderer> CreateRenderer(RenderBackend backend) = 0;
    
    // 获取支持的渲染后端列表
    virtual std::vector<RenderBackend> GetSupportedBackends() = 0;
};
```

#### 2.2.2 硬解码抽象层

```cpp
// 硬件加速类型
enum class HWAccelType {
    kVideoToolbox,      // iOS/macOS
    kMediaCodec,        // Android
    kDXVA2,             // Windows DXVA2
    kD3D11VA,           // Windows D3D11VA
    kNVDEC,             // NVIDIA
    kVAAPI,             // Linux VAAPI
    kNone               // 无硬件加速
};

// 硬解码器工厂
class IHWDecoderFactory {
public:
    virtual ~IHWDecoderFactory() = default;
    
    // 创建硬解码器
    virtual std::unique_ptr<IDecoder> CreateDecoder(HWAccelType type) = 0;
    
    // 检查是否支持特定格式
    virtual bool IsFormatSupported(AVCodecID codec_id, HWAccelType type) = 0;
    
    // 获取平台最优硬解码类型
    virtual HWAccelType GetOptimalHWAccel(AVCodecID codec_id) = 0;
};
```

#### 2.2.3 网络抽象层

```cpp
// 网络协议类型
enum class ProtocolType {
    kFile,          // 本地文件
    kHttp,          // HTTP
    kHttps,         // HTTPS
    kRtmp,          // RTMP
    kRtsp,          // RTSP
    kHls,           // HLS
    kDash,          // DASH
    kUdp,           // UDP
    kTcp            // TCP
};

// 网络IO接口
class INetworkIO {
public:
    virtual ~INetworkIO() = default;
    
    // 打开连接
    virtual int Open(const std::string& url, int timeout_ms) = 0;
    
    // 读取数据
    virtual int Read(uint8_t* buffer, int size) = 0;
    
    // 写入数据
    virtual int Write(const uint8_t* data, int size) = 0;
    
    // 关闭连接
    virtual void Close() = 0;
    
    // 获取协议类型
    virtual ProtocolType GetProtocolType() const = 0;
};
```

### 2.3 工具模块

#### 2.3.1 线程池

```cpp
// 线程池配置
struct ThreadPoolConfig {
    int min_threads = 2;
    int max_threads = 8;
    int queue_size = 100;
    int keep_alive_ms = 60000;
};

// 线程池
class ThreadPool {
public:
    explicit ThreadPool(const ThreadPoolConfig& config);
    ~ThreadPool();
    
    // 提交任务
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;
    
    // 获取等待任务数
    size_t GetPendingTaskCount() const;
    
    // 获取活跃线程数
    int GetActiveThreadCount() const;
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    ThreadPoolConfig config_;
};
```

#### 2.3.2 内存池

```cpp
// 内存块大小类别
enum class BlockSize {
    kSmall = 256,       // 小内存块
    kMedium = 4096,     // 中内存块
    kLarge = 65536,     // 大内存块
    kHuge = 1048576     // 超大内存块
};

// 内存池
class MemoryPool {
public:
    static MemoryPool& GetInstance();
    
    // 分配内存
    void* Allocate(size_t size);
    void Deallocate(void* ptr, size_t size);
    
    // 分配对齐内存
    void* AlignedAllocate(size_t size, size_t alignment);
    
    // 获取统计信息
    MemoryStats GetStats() const;
    
private:
    std::array<std::unique_ptr<FixedBlockAllocator>, 4> allocators_;
    std::mutex mutex_;
};

// 智能内存指针
using AVFramePtr = std::shared_ptr<AVFrame>;
using AVPacketPtr = std::shared_ptr<AVPacket>;
```

#### 2.3.3 日志系统

```cpp
// 日志级别
enum class LogLevel {
    kVerbose = 0,
    kDebug = 1,
    kInfo = 2,
    kWarning = 3,
    kError = 4,
    kFatal = 5
};

// 日志系统
class Logger {
public:
    static Logger& GetInstance();
    
    // 设置日志级别
    void SetLogLevel(LogLevel level);
    
    // 设置日志输出回调
    void SetOutputCallback(std::function<void(LogLevel, const std::string&)> callback);
    
    // 设置日志文件
    void SetLogFile(const std::string& file_path);
    
    // 日志输出
    void Log(LogLevel level, const std::string& tag, const std::string& message);
    
    // 宏辅助函数
    #define LOG_VERBOSE(tag, ...) Logger::GetInstance().Log(LogLevel::kVerbose, tag, fmt::format(__VA_ARGS__))
    #define LOG_DEBUG(tag, ...)   Logger::GetInstance().Log(LogLevel::kDebug, tag, fmt::format(__VA_ARGS__))
    #define LOG_INFO(tag, ...)    Logger::GetInstance().Log(LogLevel::kInfo, tag, fmt::format(__VA_ARGS__))
    #define LOG_WARNING(tag, ...) Logger::GetInstance().Log(LogLevel::kWarning, tag, fmt::format(__VA_ARGS__))
    #define LOG_ERROR(tag, ...)   Logger::GetInstance().Log(LogLevel::kError, tag, fmt::format(__VA_ARGS__))
    #define LOG_FATAL(tag, ...)   Logger::GetInstance().Log(LogLevel::kFatal, tag, fmt::format(__VA_ARGS__))
    
private:
    LogLevel level_ = LogLevel::kInfo;
    std::mutex mutex_;
    std::ofstream file_stream_;
    std::function<void(LogLevel, const std::string&)> callback_;
};
```

---

## 3. 数据流设计

### 3.1 播放数据流

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           播放数据流 (Playback Pipeline)                      │
└─────────────────────────────────────────────────────────────────────────────┘

输入源
    │
    ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Demuxer   │────▶│   Decoder   │────▶│   Filter    │────▶│  Renderer   │
│   解封装     │     │   解码      │     │   滤镜      │     │   渲染      │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
      │                   │                   │                   │
      │              ┌────┴────┐              │                   │
      │              │         │              │                   │
      │              ▼         ▼              │                   │
      │        ┌─────────┐ ┌─────────┐        │                   │
      │        │软解码器 │ │硬解码器 │        │                   │
      │        └─────────┘ └─────────┘        │                   │
      │                                       │                   │
      │                   ┌───────────────────┘                   │
      │                   │                                       │
      ▼                   ▼                                       ▼
┌─────────────┐     ┌─────────────┐                       ┌─────────────┐
│ 数据包队列   │     │  帧队列     │                       │  显示队列    │
│Packet Queue │     │ Frame Queue │                       │Display Queue│
└─────────────┘     └─────────────┘                       └─────────────┘

数据类型转换:
AVPacket ─────▶ AVFrame ─────▶ AVFrame ─────▶ 渲染数据
(压缩数据)      (原始数据)      (处理后)       (GPU纹理)
```

### 3.2 编码数据流

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           编码数据流 (Encoding Pipeline)                      │
└─────────────────────────────────────────────────────────────────────────────┘

视频采集
    │
    ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Capture   │────▶│   Encoder   │────▶│   Muxer     │────▶│   Output    │
│   采集      │     │   编码      │     │   封装      │     │   输出      │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
      │                   │                   │                   │
      │              ┌────┴────┐              │                   │
      │              │         │              │                   │
      │              ▼         ▼              │                   │
      │        ┌─────────┐ ┌─────────┐        │                   │
      │        │软编码器 │ │硬编码器 │        │                   │
      │        └─────────┘ └─────────┘        │                   │
      │                                       │                   │
      │                   ┌───────────────────┘                   │
      │                   │                                       │
      ▼                   ▼                                       ▼
┌─────────────┐     ┌─────────────┐                       ┌─────────────┐
│ 原始帧队列   │     │ 编码包队列   │                       │  文件/网络   │
│Frame Queue  │     │Packet Queue │                       │             │
└─────────────┘     └─────────────┘                       └─────────────┘

数据类型转换:
原始帧 ─────▶ AVFrame ─────▶ AVPacket ─────▶ 输出数据
(CVPixelBuffer) (处理后)     (压缩数据)      (文件/流)
```

### 3.3 数据旁路流程

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          数据旁路流程 (Data Bypass)                           │
└─────────────────────────────────────────────────────────────────────────────┘

                    ┌─────────────────────────────────────┐
                    │           数据旁路回调               │
                    │     (Data Bypass Callback)          │
                    └─────────────────────────────────────┘
                                      ▲
                                      │
    ┌─────────────────────────────────┼─────────────────────────────────┐
    │                                 │                                 │
    │    ┌─────────────┐              │              ┌─────────────┐   │
    │    │   Decoder   │──────────────┼──────────────│   Encoder   │   │
    │    │   解码后    │              │              │   编码后    │   │
    │    └─────────────┘              │              └─────────────┘   │
    │                                 │                                 │
    │    ┌─────────────┐              │              ┌─────────────┐   │
    │    │   Filter    │──────────────┼──────────────│   Muxer     │   │
    │    │   滤镜后    │              │              │   封装前    │   │
    │    └─────────────┘              │              └─────────────┘   │
    │                                 │                                 │
    └─────────────────────────────────┼─────────────────────────────────┘
                                      │
                    ┌─────────────────┴─────────────────┐
                    │         回调数据类型                │
                    │  ┌─────────────────────────────┐  │
                    │  │  RawVideoFrame - 原始视频帧  │  │
                    │  │  RawAudioFrame - 原始音频帧  │  │
                    │  │  EncodedPacket - 编码数据包  │  │
                    │  │  Metadata      - 媒体元数据  │  │
                    │  └─────────────────────────────┘  │
                    └─────────────────────────────────────┘
```

---

## 4. 线程模型设计

### 4.1 线程分配

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           线程模型 (Thread Model)                             │
└─────────────────────────────────────────────────────────────────────────────┘

主线程 (Main Thread)
    │
    ├──▶ 应用UI线程
    │     - 处理用户交互
    │     - 调用SDK API
    │     - 接收回调事件
    │
    └──▶ SDK控制线程
          - 状态管理
          - 生命周期控制
          - 回调分发

工作线程池 (Worker Thread Pool)
    │
    ├──▶ 解封装线程 (Demuxer Thread)
    │     - 读取媒体数据
    │     - 解析格式
    │     - 填充Packet队列
    │
    ├──▶ 视频解码线程 (Video Decode Thread)
    │     - 解码视频Packet
    │     - 硬件/软件解码
    │     - 填充Frame队列
    │
    ├──▶ 音频解码线程 (Audio Decode Thread)
    │     - 解码音频Packet
    │     - 音频重采样
    │     - 填充Audio队列
    │
    ├──▶ 渲染线程 (Render Thread)
    │     - 视频渲染
    │     - OpenGL/Metal/DirectX
    │     - 垂直同步
    │
    ├──▶ 音频播放线程 (Audio Play Thread)
    │     - 音频输出
    │     - 音频同步
    │
    ├──▶ 编码线程 (Encode Thread)
    │     - 视频编码
    │     - 音频编码
    │
    └──▶ 网络IO线程 (Network IO Thread)
          - 网络数据收发
          - 缓冲管理

定时器线程 (Timer Thread)
    │
    └──▶ 时钟同步线程
          - 音视频同步
          - 播放进度控制
```

### 4.2 线程间通信机制

```cpp
// 消息类型
enum class MessageType {
    kStart,         // 开始播放
    kPause,         // 暂停
    kResume,        // 恢复
    kStop,          // 停止
    kSeek,          // 定位
    kSpeedChange,   // 倍速变化
    kRenderFrame,   // 渲染帧
    kDecodeFrame,   // 解码帧
    kError,         // 错误
    kComplete,      // 播放完成
    kProgress,      // 进度更新
};

// 消息结构
struct Message {
    MessageType type;
    int64_t timestamp;
    std::any data;
};

// 消息队列
class MessageQueue {
public:
    void Push(const Message& msg);
    bool Pop(Message& msg, int timeout_ms);
    void Clear();
    size_t Size() const;
    
private:
    std::queue<Message> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

// 线程间通信管理器
class ThreadMessenger {
public:
    // 注册消息处理器
    void RegisterHandler(ThreadId id, std::function<void(const Message&)> handler);
    
    // 发送消息到指定线程
    void SendMessage(ThreadId target, const Message& msg);
    
    // 广播消息
    void BroadcastMessage(const Message& msg);
    
private:
    std::unordered_map<ThreadId, std::shared_ptr<MessageQueue>> queues_;
    std::unordered_map<ThreadId, std::function<void(const Message&)>> handlers_;
};
```

### 4.3 同步策略

```cpp
// 同步模式
enum class SyncMode {
    kVideoMaster,       // 视频为主时钟
    kAudioMaster,       // 音频为主时钟
    kExternalClock      // 外部时钟
};

// 时钟管理器
class ClockManager {
public:
    // 设置同步模式
    void SetSyncMode(SyncMode mode);
    
    // 更新视频时钟
    void UpdateVideoClock(double pts);
    
    // 更新音频时钟
    void UpdateAudioClock(double pts);
    
    // 获取主时钟
    double GetMasterClock() const;
    
    // 计算同步差值
    double GetSyncDiff() const;
    
    // 获取等待时间
    int64_t GetWaitTime(double pts) const;
    
private:
    SyncMode sync_mode_ = SyncMode::kAudioMaster;
    std::atomic<double> video_clock_{0};
    std::atomic<double> audio_clock_{0};
    std::atomic<double> external_clock_{0};
    mutable std::mutex mutex_;
};

// 队列同步控制
class QueueController {
public:
    // 设置最大队列大小
    void SetMaxSize(size_t max_size);
    
    // 检查队列是否已满
    bool IsFull() const;
    
    // 检查队列是否为空
    bool IsEmpty() const;
    
    // 等待队列有空间
    bool WaitForSpace(int timeout_ms);
    
    // 等待队列有数据
    bool WaitForData(int timeout_ms);
    
private:
    size_t max_size_ = 100;
    std::atomic<size_t> current_size_{0};
    std::mutex mutex_;
    std::condition_variable cond_space_;
    std::condition_variable cond_data_;
};
```

---

## 5. 跨平台抽象层设计

### 5.1 渲染抽象接口

```cpp
// 渲染上下文
struct RenderContext {
    void* native_window = nullptr;      // 原生窗口句柄
    void* shared_context = nullptr;     // 共享上下文
    int width = 0;
    int height = 0;
    int color_depth = 8;
};

// 着色器类型
enum class ShaderType {
    kVertex,            // 顶点着色器
    kFragment,          // 片段着色器
    kCompute            // 计算着色器
};

// 纹理格式
enum class TextureFormat {
    kRGBA8,             // RGBA 8-bit
    kBGRA8,             // BGRA 8-bit
    kYUV420P,           // YUV 4:2:0 Planar
    kNV12,              // YUV 4:2:0 Semi-Planar
    kNV21               // YVU 4:2:0 Semi-Planar
};

// 渲染抽象基类
class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;
    
    // 初始化/释放
    virtual int Initialize(const RenderContext& context) = 0;
    virtual void Release() = 0;
    
    // 帧缓冲操作
    virtual int CreateFramebuffer(int width, int height) = 0;
    virtual void DeleteFramebuffer(int fb_id) = 0;
    virtual void BindFramebuffer(int fb_id) = 0;
    
    // 纹理操作
    virtual int CreateTexture(TextureFormat format, int width, int height) = 0;
    virtual void DeleteTexture(int tex_id) = 0;
    virtual void UpdateTexture(int tex_id, const void* data, int width, int height) = 0;
    virtual void BindTexture(int tex_id, int slot) = 0;
    
    // 着色器操作
    virtual int CreateShader(ShaderType type, const std::string& source) = 0;
    virtual int CreateProgram(int vertex_shader, int fragment_shader) = 0;
    virtual void UseProgram(int program_id) = 0;
    virtual void DeleteShader(int shader_id) = 0;
    virtual void DeleteProgram(int program_id) = 0;
    
    // 渲染操作
    virtual void Clear(float r, float g, float b, float a) = 0;
    virtual void DrawArrays(int first, int count) = 0;
    virtual void DrawElements(int count) = 0;
    virtual void SwapBuffers() = 0;
    
    // 视口设置
    virtual void SetViewport(int x, int y, int width, int height) = 0;
    
    // 获取错误信息
    virtual std::string GetError() = 0;
    
    // 获取后端类型
    virtual RenderBackend GetType() const = 0;
};

// 平台渲染器工厂
class RenderBackendFactory {
public:
    // 创建渲染后端
    static std::unique_ptr<IRenderBackend> CreateBackend(RenderBackend type);
    
    // 获取平台默认渲染后端
    static RenderBackend GetDefaultBackend();
    
    // 获取平台支持的渲染后端列表
    static std::vector<RenderBackend> GetSupportedBackends();
};
```

### 5.2 硬解码抽象接口

```cpp
// 硬解码配置
struct HWDecoderConfig {
    AVCodecID codec_id = AV_CODEC_ID_NONE;
    int width = 0;
    int height = 0;
    AVPixelFormat pixel_format = AV_PIX_FMT_NONE;
    void* native_context = nullptr;     // 平台特定上下文
};

// 硬解码设备信息
struct HWDeviceInfo {
    std::string name;
    HWAccelType type;
    std::vector<AVCodecID> supported_codecs;
    std::vector<AVPixelFormat> supported_formats;
    int max_width = 0;
    int max_height = 0;
};

// 硬解码器抽象接口
class IHWDecoder {
public:
    virtual ~IHWDecoder() = default;
    
    // 初始化
    virtual int Initialize(const HWDecoderConfig& config) = 0;
    
    // 解码
    virtual int Decode(const AVPacket* packet, AVFrame* frame) = 0;
    
    // 刷新
    virtual void Flush() = 0;
    
    // 关闭
    virtual void Close() = 0;
    
    // 获取设备信息
    virtual HWDeviceInfo GetDeviceInfo() const = 0;
    
    // 获取硬解码类型
    virtual HWAccelType GetType() const = 0;
    
    // 获取输出格式
    virtual AVPixelFormat GetOutputFormat() const = 0;
};

// VideoToolbox解码器 (iOS/macOS)
class VideoToolboxDecoder : public IHWDecoder {
public:
    int Initialize(const HWDecoderConfig& config) override;
    int Decode(const AVPacket* packet, AVFrame* frame) override;
    void Flush() override;
    void Close() override;
    HWDeviceInfo GetDeviceInfo() const override;
    HWAccelType GetType() const override { return HWAccelType::kVideoToolbox; }
    AVPixelFormat GetOutputFormat() const override;
    
private:
    VTDecompressionSessionRef session_ = nullptr;
    CMVideoFormatDescriptionRef format_desc_ = nullptr;
    CVPixelBufferPoolRef pixel_buffer_pool_ = nullptr;
};

// MediaCodec解码器 (Android)
class MediaCodecDecoder : public IHWDecoder {
public:
    int Initialize(const HWDecoderConfig& config) override;
    int Decode(const AVPacket* packet, AVFrame* frame) override;
    void Flush() override;
    void Close() override;
    HWDeviceInfo GetDeviceInfo() const override;
    HWAccelType GetType() const override { return HWAccelType::kMediaCodec; }
    AVPixelFormat GetOutputFormat() const override;
    
private:
    AMediaCodec* codec_ = nullptr;
    AMediaFormat* format_ = nullptr;
    ANativeWindow* native_window_ = nullptr;
};

// DXVA2/D3D11VA解码器 (Windows)
class DXVADecoder : public IHWDecoder {
public:
    int Initialize(const HWDecoderConfig& config) override;
    int Decode(const AVPacket* packet, AVFrame* frame) override;
    void Flush() override;
    void Close() override;
    HWDeviceInfo GetDeviceInfo() const override;
    HWAccelType GetType() const override { return HWAccelType::kDXVA2; }
    AVPixelFormat GetOutputFormat() const override;
    
private:
    IDirect3DDevice9* d3d_device_ = nullptr;
    IDirectXVideoDecoder* decoder_ = nullptr;
    DXVA2_ConfigPictureDecode config_;
};

// 硬解码器工厂
class HWDecoderFactory {
public:
    // 创建硬解码器
    static std::unique_ptr<IHWDecoder> CreateDecoder(HWAccelType type);
    
    // 获取平台可用的硬解码器类型
    static std::vector<HWAccelType> GetAvailableDecoders();
    
    // 检查是否支持特定编解码器
    static bool IsCodecSupported(AVCodecID codec_id, HWAccelType type);
    
    // 获取最优硬解码器
    static HWAccelType GetOptimalDecoder(AVCodecID codec_id);
};
```

### 5.3 网络抽象接口

```cpp
// 网络配置
struct NetworkConfig {
    int connect_timeout_ms = 10000;     // 连接超时
    int read_timeout_ms = 5000;         // 读取超时
    int write_timeout_ms = 5000;        // 写入超时
    int buffer_size = 65536;            // 缓冲区大小
    bool use_https = false;             // 使用HTTPS
    std::map<std::string, std::string> headers;  // HTTP头
};

// 连接状态
enum class ConnectionState {
    kDisconnected,
    kConnecting,
    kConnected,
    kError
};

// 网络回调
class INetworkCallback {
public:
    virtual ~INetworkCallback() = default;
    
    virtual void OnConnected() = 0;
    virtual void OnDisconnected() = 0;
    virtual void OnDataReceived(const uint8_t* data, int size) = 0;
    virtual void OnError(int error_code, const std::string& error_msg) = 0;
};

// 网络IO抽象接口
class INetworkIO {
public:
    virtual ~INetworkIO() = default;
    
    // 连接管理
    virtual int Connect(const std::string& url, const NetworkConfig& config) = 0;
    virtual void Disconnect() = 0;
    virtual ConnectionState GetState() const = 0;
    
    // 数据读写
    virtual int Read(uint8_t* buffer, int size) = 0;
    virtual int Write(const uint8_t* data, int size) = 0;
    virtual int ReadAsync(uint8_t* buffer, int size, std::function<void(int)> callback) = 0;
    virtual int WriteAsync(const uint8_t* data, int size, std::function<void(int)> callback) = 0;
    
    // 设置回调
    virtual void SetCallback(INetworkCallback* callback) = 0;
    
    // 获取协议类型
    virtual ProtocolType GetProtocolType() const = 0;
    
    // 获取统计信息
    virtual NetworkStats GetStats() const = 0;
};

// HTTP/HTTPS实现
class HttpNetworkIO : public INetworkIO {
public:
    int Connect(const std::string& url, const NetworkConfig& config) override;
    void Disconnect() override;
    ConnectionState GetState() const override;
    int Read(uint8_t* buffer, int size) override;
    int Write(const uint8_t* data, int size) override;
    int ReadAsync(uint8_t* buffer, int size, std::function<void(int)> callback) override;
    int WriteAsync(const uint8_t* data, int size, std::function<void(int)> callback) override;
    void SetCallback(INetworkCallback* callback) override;
    ProtocolType GetProtocolType() const override;
    NetworkStats GetStats() const override;
    
private:
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl_{nullptr, curl_easy_cleanup};
    ConnectionState state_ = ConnectionState::kDisconnected;
    NetworkConfig config_;
    INetworkCallback* callback_ = nullptr;
};

// RTMP实现
class RtmpNetworkIO : public INetworkIO {
public:
    int Connect(const std::string& url, const NetworkConfig& config) override;
    void Disconnect() override;
    ConnectionState GetState() const override;
    int Read(uint8_t* buffer, int size) override;
    int Write(const uint8_t* data, int size) override;
    int ReadAsync(uint8_t* buffer, int size, std::function<void(int)> callback) override;
    int WriteAsync(const uint8_t* data, int size, std::function<void(int)> callback) override;
    void SetCallback(INetworkCallback* callback) override;
    ProtocolType GetProtocolType() const override { return ProtocolType::kRtmp; }
    NetworkStats GetStats() const override;
    
private:
    RTMP* rtmp_ = nullptr;
    ConnectionState state_ = ConnectionState::kDisconnected;
};

// 网络工厂
class NetworkFactory {
public:
    // 根据URL创建网络IO
    static std::unique_ptr<INetworkIO> CreateIO(const std::string& url);
    
    // 创建特定协议的网络IO
    static std::unique_ptr<INetworkIO> CreateIO(ProtocolType type);
    
    // 获取URL对应的协议类型
    static ProtocolType GetProtocolType(const std::string& url);
};
```

---

## 6. 内存管理策略

### 6.1 内存池设计

```cpp
// 内存池配置
struct MemoryPoolConfig {
    size_t small_block_size = 256;          // 小内存块
    size_t small_block_count = 1024;
    size_t medium_block_size = 4096;        // 中内存块
    size_t medium_block_count = 256;
    size_t large_block_size = 65536;        // 大内存块
    size_t large_block_count = 64;
    size_t huge_block_size = 1048576;       // 超大内存块
    size_t huge_block_count = 16;
};

// 固定大小内存分配器
class FixedBlockAllocator {
public:
    FixedBlockAllocator(size_t block_size, size_t block_count);
    ~FixedBlockAllocator();
    
    void* Allocate();
    void Deallocate(void* ptr);
    
    size_t GetBlockSize() const { return block_size_; }
    size_t GetFreeCount() const;
    size_t GetUsedCount() const;
    
private:
    size_t block_size_;
    size_t block_count_;
    std::vector<uint8_t> memory_;
    std::stack<void*> free_list_;
    std::atomic<size_t> used_count_{0};
    std::mutex mutex_;
};

// 内存池主类
class MemoryPool {
public:
    static MemoryPool& GetInstance();
    
    void Initialize(const MemoryPoolConfig& config);
    void Release();
    
    // 分配内存
    void* Allocate(size_t size);
    void Deallocate(void* ptr, size_t size);
    
    // 分配对齐内存
    void* AlignedAllocate(size_t size, size_t alignment);
    void AlignedDeallocate(void* ptr);
    
    // 获取统计信息
    struct Stats {
        size_t total_allocated;
        size_t total_used;
        size_t total_free;
        size_t allocation_count;
        size_t deallocation_count;
    };
    Stats GetStats() const;
    
private:
    MemoryPool() = default;
    ~MemoryPool() = default;
    
    std::array<std::unique_ptr<FixedBlockAllocator>, 4> allocators_;
    std::mutex fallback_mutex_;
    std::unordered_map<void*, size_t> fallback_allocations_;
};

// 智能指针类型
using AVFramePtr = std::shared_ptr<AVFrame>;
using AVPacketPtr = std::shared_ptr<AVPacket>;

// 内存池分配器
class PoolAllocator {
public:
    static void* Allocate(size_t size);
    static void Deallocate(void* ptr, size_t size);
};
```

### 6.2 引用计数机制

```cpp
// 引用计数基类
class RefCounted {
public:
    RefCounted() = default;
    virtual ~RefCounted() = default;
    
    void AddRef() const {
        ref_count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    void Release() const {
        if (ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }
    
    int GetRefCount() const {
        return ref_count_.load(std::memory_order_relaxed);
    }
    
private:
    mutable std::atomic<int> ref_count_{0};
};

// 引用计数智能指针
template<typename T>
class RefPtr {
public:
    RefPtr() = default;
    RefPtr(std::nullptr_t) {}
    
    explicit RefPtr(T* ptr) : ptr_(ptr) {
        if (ptr_) {
            ptr_->AddRef();
        }
    }
    
    RefPtr(const RefPtr& other) : ptr_(other.ptr_) {
        if (ptr_) {
            ptr_->AddRef();
        }
    }
    
    RefPtr(RefPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    ~RefPtr() {
        if (ptr_) {
            ptr_->Release();
        }
    }
    
    RefPtr& operator=(const RefPtr& other) {
        if (this != &other) {
            if (ptr_) {
                ptr_->Release();
            }
            ptr_ = other.ptr_;
            if (ptr_) {
                ptr_->AddRef();
            }
        }
        return *this;
    }
    
    RefPtr& operator=(RefPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                ptr_->Release();
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* Get() const { return ptr_; }
    
    explicit operator bool() const { return ptr_ != nullptr; }
    
    bool operator==(const RefPtr& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const RefPtr& other) const { return ptr_ != other.ptr_; }
    
    void Reset(T* ptr = nullptr) {
        if (ptr_) {
            ptr_->Release();
        }
        ptr_ = ptr;
        if (ptr_) {
            ptr_->AddRef();
        }
    }
    
private:
    T* ptr_ = nullptr;
};

// FFmpeg对象包装器
class AVFrameWrapper : public RefCounted {
public:
    AVFrameWrapper() {
        frame_ = av_frame_alloc();
    }
    
    explicit AVFrameWrapper(AVFrame* frame) : frame_(frame) {}
    
    ~AVFrameWrapper() override {
        if (frame_) {
            av_frame_free(&frame_);
        }
    }
    
    AVFrame* Get() const { return frame_; }
    AVFrame* operator->() const { return frame_; }
    
private:
    AVFrame* frame_ = nullptr;
};

class AVPacketWrapper : public RefCounted {
public:
    AVPacketWrapper() {
        packet_ = av_packet_alloc();
    }
    
    explicit AVPacketWrapper(AVPacket* packet) : packet_(packet) {}
    
    ~AVPacketWrapper() override {
        if (packet_) {
            av_packet_free(&packet_);
        }
    }
    
    AVPacket* Get() const { return packet_; }
    AVPacket* operator->() const { return packet_; }
    
private:
    AVPacket* packet_ = nullptr;
};
```

### 6.3 零拷贝优化

```cpp
// 零拷贝缓冲区
class ZeroCopyBuffer {
public:
    // 创建从硬件缓冲区
    static std::unique_ptr<ZeroCopyBuffer> FromHardwareBuffer(void* hw_buffer);
    
    // 创建从内存
    static std::unique_ptr<ZeroCopyBuffer> FromMemory(void* data, size_t size);
    
    // 获取数据指针
    virtual void* GetData() = 0;
    virtual size_t GetSize() = 0;
    
    // 映射到CPU可访问内存
    virtual void* Map() = 0;
    virtual void Unmap() = 0;
    
    // 获取硬件缓冲区
    virtual void* GetHardwareBuffer() = 0;
    
    virtual ~ZeroCopyBuffer() = default;
};

// 视频帧缓冲区
class VideoFrameBuffer : public ZeroCopyBuffer {
public:
    // 从CVPixelBuffer创建 (iOS/macOS)
    static std::unique_ptr<VideoFrameBuffer> FromCVPixelBuffer(CVPixelBufferRef pixel_buffer);
    
    // 从AHardwareBuffer创建 (Android)
    static std::unique_ptr<VideoFrameBuffer> FromAHardwareBuffer(AHardwareBuffer* buffer);
    
    // 从ID3D11Texture2D创建 (Windows)
    static std::unique_ptr<VideoFrameBuffer> FromD3D11Texture(ID3D11Texture2D* texture);
    
    // 获取平面数据
    virtual void* GetPlaneData(int plane) = 0;
    virtual int GetPlaneStride(int plane) = 0;
    virtual int GetPlaneSize(int plane) = 0;
    
    // 获取像素格式
    virtual AVPixelFormat GetPixelFormat() = 0;
};

// 零拷贝数据流
class ZeroCopyStream {
public:
    // 从解码器直接获取硬件缓冲区
    virtual std::unique_ptr<VideoFrameBuffer> AcquireFrame() = 0;
    
    // 将硬件缓冲区传递给渲染器
    virtual int RenderFrame(VideoFrameBuffer* buffer) = 0;
    
    // 释放硬件缓冲区
    virtual void ReleaseFrame(VideoFrameBuffer* buffer) = 0;
};

// 纹理上传优化
class TextureUploader {
public:
    // 使用PBO进行异步纹理上传 (OpenGL)
    int UploadWithPBO(const uint8_t* data, int width, int height, GLuint texture);
    
    // 使用共享纹理 (Metal/DirectX)
    int UploadSharedTexture(void* native_texture, int width, int height);
    
    // 使用DMA-BUF (Linux)
    int UploadWithDMABuf(int dmabuf_fd, int width, int height);
};
```

---

## 7. 扩展性设计

### 7.1 支持新的编码格式

```cpp
// 编解码器插件接口
class ICodecPlugin {
public:
    virtual ~ICodecPlugin() = default;
    
    // 获取支持的编解码器ID列表
    virtual std::vector<AVCodecID> GetSupportedCodecs() = 0;
    
    // 创建解码器
    virtual std::unique_ptr<IDecoder> CreateDecoder(AVCodecID codec_id) = 0;
    
    // 创建编码器
    virtual std::unique_ptr<IEncoder> CreateEncoder(AVCodecID codec_id) = 0;
    
    // 获取插件名称
    virtual std::string GetName() const = 0;
    
    // 获取插件版本
    virtual std::string GetVersion() const = 0;
};

// 编解码器插件管理器
class CodecPluginManager {
public:
    static CodecPluginManager& GetInstance();
    
    // 注册插件
    void RegisterPlugin(std::unique_ptr<ICodecPlugin> plugin);
    
    // 注销插件
    void UnregisterPlugin(const std::string& name);
    
    // 创建解码器
    std::unique_ptr<IDecoder> CreateDecoder(AVCodecID codec_id);
    
    // 创建编码器
    std::unique_ptr<IEncoder> CreateEncoder(AVCodecID codec_id);
    
    // 获取所有支持的编解码器
    std::vector<AVCodecID> GetAllSupportedCodecs();
    
private:
    std::vector<std::unique_ptr<ICodecPlugin>> plugins_;
    std::mutex mutex_;
};

// 示例：H.266/VVC解码器插件
class VVCDecoderPlugin : public ICodecPlugin {
public:
    std::vector<AVCodecID> GetSupportedCodecs() override {
        return {AV_CODEC_ID_VVC};
    }
    
    std::unique_ptr<IDecoder> CreateDecoder(AVCodecID codec_id) override {
        if (codec_id == AV_CODEC_ID_VVC) {
            return std::make_unique<VVCDecoder>();
        }
        return nullptr;
    }
    
    std::unique_ptr<IEncoder> CreateEncoder(AVCodecID codec_id) override {
        if (codec_id == AV_CODEC_ID_VVC) {
            return std::make_unique<VVCEncoder>();
        }
        return nullptr;
    }
    
    std::string GetName() const override { return "VVC Plugin"; }
    std::string GetVersion() const override { return "1.0.0"; }
};

// 注册插件
void RegisterBuiltinPlugins() {
    auto& manager = CodecPluginManager::GetInstance();
    manager.RegisterPlugin(std::make_unique<VVCDecoderPlugin>());
    // 注册其他插件...
}
```

### 7.2 支持新的平台

```cpp
// 平台适配接口
class IPlatformAdapter {
public:
    virtual ~IPlatformAdapter() = default;
    
    // 初始化平台
    virtual int Initialize() = 0;
    
    // 创建渲染器
    virtual std::unique_ptr<IRenderer> CreateRenderer() = 0;
    
    // 创建硬解码器
    virtual std::unique_ptr<IHWDecoder> CreateHWDecoder(HWAccelType type) = 0;
    
    // 创建采集器
    virtual std::unique_ptr<ICapture> CreateCapture() = 0;
    
    // 获取平台名称
    virtual std::string GetPlatformName() const = 0;
    
    // 获取支持的功能
    virtual PlatformCapabilities GetCapabilities() const = 0;
};

// 平台能力
struct PlatformCapabilities {
    bool supports_hardware_decode = false;
    bool supports_hardware_encode = false;
    bool supports_opengl_es = false;
    bool supports_metal = false;
    bool supports_directx = false;
    bool supports_vulkan = false;
    std::vector<HWAccelType> supported_hw_accels;
    std::vector<RenderBackend> supported_render_backends;
    int max_texture_size = 0;
    int max_video_resolution = 0;
};

// 平台适配器工厂
class PlatformAdapterFactory {
public:
    static std::unique_ptr<IPlatformAdapter> CreateAdapter();
    
    static PlatformType GetCurrentPlatform();
};

// iOS平台适配器
class iOSPlatformAdapter : public IPlatformAdapter {
public:
    int Initialize() override;
    std::unique_ptr<IRenderer> CreateRenderer() override;
    std::unique_ptr<IHWDecoder> CreateHWDecoder(HWAccelType type) override;
    std::unique_ptr<ICapture> CreateCapture() override;
    std::string GetPlatformName() const override { return "iOS"; }
    PlatformCapabilities GetCapabilities() const override;
};

// Android平台适配器
class AndroidPlatformAdapter : public IPlatformAdapter {
public:
    int Initialize() override;
    std::unique_ptr<IRenderer> CreateRenderer() override;
    std::unique_ptr<IHWDecoder> CreateHWDecoder(HWAccelType type) override;
    std::unique_ptr<ICapture> CreateCapture() override;
    std::string GetPlatformName() const override { return "Android"; }
    PlatformCapabilities GetCapabilities() const override;
};

// macOS平台适配器
class macOSPlatformAdapter : public IPlatformAdapter {
public:
    int Initialize() override;
    std::unique_ptr<IRenderer> CreateRenderer() override;
    std::unique_ptr<IHWDecoder> CreateHWDecoder(HWAccelType type) override;
    std::unique_ptr<ICapture> CreateCapture() override;
    std::string GetPlatformName() const override { return "macOS"; }
    PlatformCapabilities GetCapabilities() const override;
};

// Windows平台适配器
class WindowsPlatformAdapter : public IPlatformAdapter {
public:
    int Initialize() override;
    std::unique_ptr<IRenderer> CreateRenderer() override;
    std::unique_ptr<IHWDecoder> CreateHWDecoder(HWAccelType type) override;
    std::unique_ptr<ICapture> CreateCapture() override;
    std::string GetPlatformName() const override { return "Windows"; }
    PlatformCapabilities GetCapabilities() const override;
};

// 平台适配器实现
std::unique_ptr<IPlatformAdapter> PlatformAdapterFactory::CreateAdapter() {
    #if defined(__APPLE__)
        #if TARGET_OS_IPHONE
            return std::make_unique<iOSPlatformAdapter>();
        #else
            return std::make_unique<macOSPlatformAdapter>();
        #endif
    #elif defined(__ANDROID__)
        return std::make_unique<AndroidPlatformAdapter>();
    #elif defined(_WIN32)
        return std::make_unique<WindowsPlatformAdapter>();
    #else
        return nullptr;
    #endif
}
```

### 7.3 插件机制

```cpp
// 插件基类
class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    // 初始化插件
    virtual int Initialize() = 0;
    
    // 释放插件
    virtual void Release() = 0;
    
    // 获取插件信息
    virtual PluginInfo GetInfo() const = 0;
};

// 插件信息
struct PluginInfo {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::vector<std::string> dependencies;
};

// 滤镜插件接口
class IFilterPlugin : public IPlugin {
public:
    // 创建滤镜
    virtual std::unique_ptr<IFilter> CreateFilter(const std::string& name) = 0;
    
    // 获取支持的滤镜列表
    virtual std::vector<std::string> GetSupportedFilters() = 0;
};

// 渲染特效插件接口
class IEffectPlugin : public IPlugin {
public:
    // 创建特效
    virtual std::unique_ptr<IEffect> CreateEffect(const std::string& name) = 0;
    
    // 获取支持的特效列表
    virtual std::vector<std::string> GetSupportedEffects() = 0;
};

// 插件管理器
class PluginManager {
public:
    static PluginManager& GetInstance();
    
    // 加载插件
    int LoadPlugin(const std::string& path);
    
    // 卸载插件
    void UnloadPlugin(const std::string& name);
    
    // 获取插件
    IPlugin* GetPlugin(const std::string& name);
    
    // 获取所有插件
    std::vector<IPlugin*> GetAllPlugins();
    
    // 获取特定类型的插件
    template<typename T>
    std::vector<T*> GetPluginsByType();
    
private:
    std::unordered_map<std::string, std::unique_ptr<IPlugin>> plugins_;
    std::mutex mutex_;
};

// 滤镜基类
class IFilter {
public:
    virtual ~IFilter() = default;
    
    // 初始化滤镜
    virtual int Initialize(const FilterConfig& config) = 0;
    
    // 处理帧
    virtual int ProcessFrame(AVFrame* input, AVFrame* output) = 0;
    
    // 释放滤镜
    virtual void Release() = 0;
    
    // 获取滤镜名称
    virtual std::string GetName() const = 0;
};

// 滤镜链
class FilterChain {
public:
    // 添加滤镜
    void AddFilter(std::unique_ptr<IFilter> filter);
    
    // 移除滤镜
    void RemoveFilter(const std::string& name);
    
    // 处理帧
    int ProcessFrame(AVFrame* frame);
    
    // 清空滤镜链
    void Clear();
    
private:
    std::vector<std::unique_ptr<IFilter>> filters_;
};
```

---

## 8. 接口设计

### 8.1 SDK主接口

```cpp
// SDK配置
struct SDKConfig {
    LogLevel log_level = LogLevel::kInfo;
    std::string log_file;
    int max_buffer_size = 100 * 1024 * 1024;  // 100MB
    bool enable_hardware_decode = true;
    bool enable_hardware_encode = true;
    RenderBackend preferred_render_backend = RenderBackend::kAuto;
};

// SDK主类
class MediaSDK {
public:
    // 初始化SDK
    static int Initialize(const SDKConfig& config);
    
    // 释放SDK
    static void Release();
    
    // 获取版本信息
    static std::string GetVersion();
    
    // 创建播放器
    static std::unique_ptr<IMediaPlayer> CreatePlayer();
    
    // 创建编码器
    static std::unique_ptr<IMediaEncoder> CreateEncoder();
    
    // 创建采集器
    static std::unique_ptr<IMediaCapture> CreateCapture();
    
    // 设置日志回调
    static void SetLogCallback(std::function<void(LogLevel, const std::string&)> callback);
    
    // 获取SDK能力
    static SDKCapabilities GetCapabilities();
};

// 播放器接口
class IMediaPlayer {
public:
    virtual ~IMediaPlayer() = default;
    
    // 打开媒体
    virtual int Open(const std::string& url) = 0;
    
    // 关闭媒体
    virtual void Close() = 0;
    
    // 播放控制
    virtual int Play() = 0;
    virtual int Pause() = 0;
    virtual int Resume() = 0;
    virtual int Stop() = 0;
    
    // 定位
    virtual int Seek(int64_t timestamp_ms) = 0;
    
    // 设置渲染窗口
    virtual int SetRenderWindow(void* native_window) = 0;
    
    // 设置渲染区域
    virtual void SetRenderRect(int x, int y, int width, int height) = 0;
    
    // 设置播放速度
    virtual void SetSpeed(float speed) = 0;
    
    // 设置音量
    virtual void SetVolume(float volume) = 0;
    
    // 获取播放状态
    virtual PlayerState GetState() const = 0;
    
    // 获取当前时间
    virtual int64_t GetCurrentTime() const = 0;
    
    // 获取总时长
    virtual int64_t GetDuration() const = 0;
    
    // 获取媒体信息
    virtual MediaInfo GetMediaInfo() const = 0;
    
    // 设置回调
    virtual void SetCallback(IMediaPlayerCallback* callback) = 0;
    
    // 设置数据旁路回调
    virtual void SetDataBypassCallback(IDataBypassCallback* callback) = 0;
};

// 播放器回调接口
class IMediaPlayerCallback {
public:
    virtual ~IMediaPlayerCallback() = default;
    
    virtual void OnPrepared() = 0;
    virtual void OnPlaying() = 0;
    virtual void OnPaused() = 0;
    virtual void OnStopped() = 0;
    virtual void OnCompleted() = 0;
    virtual void OnError(int error_code, const std::string& error_msg) = 0;
    virtual void OnProgress(int64_t current_time, int64_t duration) = 0;
    virtual void OnBuffering(int percent) = 0;
    virtual void OnSeekComplete() = 0;
};

// 数据旁路回调接口
class IDataBypassCallback {
public:
    virtual ~IDataBypassCallback() = default;
    
    // 解码后视频帧回调
    virtual void OnDecodedVideoFrame(const AVFrame* frame, int64_t timestamp) = 0;
    
    // 解码后音频帧回调
    virtual void OnDecodedAudioFrame(const AVFrame* frame, int64_t timestamp) = 0;
    
    // 编码后数据包回调
    virtual void OnEncodedPacket(const AVPacket* packet, int64_t timestamp) = 0;
    
    // 元数据回调
    virtual void OnMetadata(const MediaInfo& info) = 0;
};

// 编码器接口
class IMediaEncoder {
public:
    virtual ~IMediaEncoder() = default;
    
    // 初始化
    virtual int Initialize(const EncoderConfig& config) = 0;
    
    // 开始编码
    virtual int Start() = 0;
    
    // 停止编码
    virtual int Stop() = 0;
    
    // 发送视频帧
    virtual int SendVideoFrame(const AVFrame* frame) = 0;
    
    // 发送音频帧
    virtual int SendAudioFrame(const AVFrame* frame) = 0;
    
    // 设置输出回调
    virtual void SetOutputCallback(IEncodeOutputCallback* callback) = 0;
    
    // 设置数据旁路回调
    virtual void SetDataBypassCallback(IDataBypassCallback* callback) = 0;
};
```

---

## 9. 开发路线图

### 9.1 四阶段实现计划

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           开发路线图 (Roadmap)                               │
└─────────────────────────────────────────────────────────────────────────────┘

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
    │     - DASH支持
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
    ├──▶ 数据旁路
    │     - 解码后数据回调
    │     - 编码后数据回调
    │     - 元数据回调
    │
    ├──▶ 高级功能
    │     - 滤镜链
    │     - 特效处理
    │     - 自定义处理
    │
    └──▶ 完整功能验证
```

---

## 10. 目录结构

```
media_sdk/
├── CMakeLists.txt              # 主构建配置
├── README.md                   # 项目说明
├── docs/                       # 文档目录
│   ├── architecture.md         # 架构设计文档
│   ├── api_reference.md        # API参考
│   └── development_guide.md    # 开发指南
├── include/                    # 公共头文件
│   └── media_sdk/
│       ├── media_sdk.h         # SDK主接口
│       ├── player.h            # 播放器接口
│       ├── encoder.h           # 编码器接口
│       └── common.h            # 公共定义
├── src/                        # 源代码
│   ├── core/                   # 核心层
│   │   ├── demuxer/            # 解封装
│   │   ├── decoder/            # 解码
│   │   ├── encoder/            # 编码
│   │   ├── muxer/              # 封装
│   │   ├── renderer/           # 渲染
│   │   ├── filter/             # 滤镜
│   │   └── clock/              # 时钟同步
│   ├── platform/               # 平台适配层
│   │   ├── ios/                # iOS实现
│   │   ├── android/            # Android实现
│   │   ├── macos/              # macOS实现
│   │   └── windows/            # Windows实现
│   ├── infrastructure/         # 基础设施
│   │   ├── thread_pool/        # 线程池
│   │   ├── memory_pool/        # 内存池
│   │   ├── logger/             # 日志系统
│   │   └── utils/              # 工具类
│   └── ffmpeg/                 # FFmpeg集成
├── third_party/                # 第三方库
│   ├── ffmpeg/                 # FFmpeg
│   ├── googletest/             # 测试框架
│   └── ...
├── tests/                      # 测试代码
│   ├── unit_tests/             # 单元测试
│   ├── integration_tests/      # 集成测试
│   └── performance_tests/      # 性能测试
├── examples/                   # 示例代码
│   ├── simple_player/          # 简单播放器
│   ├── encoder_demo/           # 编码示例
│   └── ...
└── bindings/                   # 语言绑定
    ├── objc/                   # Objective-C绑定
    ├── java/                   # Java绑定
    └── csharp/                 # C#绑定
```

---

## 11. 总结

本架构设计文档详细描述了跨平台音视频SDK的整体架构，包括：

1. **分层架构**：应用层、平台适配层、核心层、基础设施层
2. **模块划分**：Demuxer、Decoder、Encoder、Muxer、Renderer等核心模块
3. **数据流设计**：播放流、编码流、数据旁路流程
4. **线程模型**：多线程设计、线程间通信、同步策略
5. **跨平台抽象**：渲染抽象、硬解码抽象、网络抽象
6. **内存管理**：内存池、引用计数、零拷贝优化
7. **扩展性设计**：新编码格式支持、新平台支持、插件机制

该架构设计遵循以下原则：
- **高内聚低耦合**：模块职责清晰，接口定义明确
- **可扩展性**：支持新格式、新平台的快速接入
- **性能优化**：零拷贝、硬件加速、内存池等技术
- **跨平台**：统一的抽象接口，平台特定实现
