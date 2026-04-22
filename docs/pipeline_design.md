# HorsAVSDK 数据处理管线设计

## 概述

本文档定义 HorsAVSDK 的音频和视频数据处理管线，涵盖播放（Playback）和采集/编码（Capture/Encoding）两大场景。设计参考业界标准 RTC SDK 的音频处理管线，同时结合 HorsAVSDK 的跨平台特性和 FFmpeg 架构。

---

## 1. 音频数据处理管线

### 1.1 音频播放管线（Audio Playback Pipeline）

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           音频播放管线 (Audio Playback)                           │
└─────────────────────────────────────────────────────────────────────────────────┘

  网络/本地源                    解码阶段                    处理阶段
  ┌─────────┐                ┌──────────────┐           ┌──────────────────┐
  │ HTTP/   │                │ FFmpeg       │           │ 音频处理链        │
  │ HLS/    │───────────────▶│ Audio        │──────────▶│ • 重采样(SWR)    │
  │ File    │   封装数据      │ Decoder      │  PCM数据   │ • 音效处理        │
  └─────────┘                │ (AAC/Opus)   │           │ • 音量调节        │
                             └──────────────┘           │ • 3A处理(可选)   │
                                                        └────────┬─────────┘
                                                                 │
                                                                 ▼
  平台音频输出              硬件抽象层                   混音阶段
  ┌────────────┐       ┌──────────────┐           ┌──────────────────┐
  │ AudioUnit  │◀──────│ IAudio       │◀─────────│ Audio Mixer      │
  │ (macOS/iOS)│       │ Renderer     │  PCM数据  │ • 多路混音        │
  │            │       │ Interface    │           │ • 耳返混合        │
  │ DirectSound│◀──────│              │◀─────────│ • 系统音频混合   │
  │ (Windows)  │       │              │           └──────────────────┘
  │ AAudio     │◀──────│              │
  │ (Android)  │       └──────────────┘
  └────────────┘

  旁路输出 (Phase 4)
  ┌────────────────────────────────────────┐
  │ DataBypassManager                      │
  │ • onAudioFrameDecoded()                │
  │ • onAudioFrameProcessed()              │
  │ • onAudioFrameRendered()               │
  └────────────────────────────────────────┘
```

#### 音频播放流程说明

| 阶段 | 组件 | 功能 | 输出格式 |
|------|------|------|----------|
| **输入源** | NetworkDemuxer/FFmpegDemuxer | 从网络(HLS/HTTP)或本地文件读取封装数据 | 原始封装数据(AAC/Opus/MP3) |
| **解码** | FFmpeg Audio Decoder | 解码压缩音频为原始 PCM | PCM (FLTP/S16) |
| **处理** | Audio Resampler (SWR) | 重采样到设备支持的格式 | PCM (S16) |
| **混音** | Audio Mixer | 支持背景音乐、耳返混合 | 混合后的 PCM |
| **渲染** | IAudioRenderer 实现 | 平台特定音频输出 | - |
| **旁路** | DataBypassManager | 回调解码后/处理后音频帧 | PCM 数据 |

#### 关键接口

```cpp
// 音频播放核心接口
class IAudioRenderer {
public:
    virtual ErrorCode Initialize(const AudioFormat& format) = 0;
    virtual ErrorCode WriteAudio(const uint8_t* data, size_t size) = 0;
    virtual ErrorCode Play() = 0;
    virtual ErrorCode Pause() = 0;
    virtual ErrorCode Stop() = 0;
    virtual ~IAudioRenderer() = default;
};

// 音频处理链
class IAudioProcessor {
public:
    virtual AudioFrame Process(const AudioFrame& input) = 0;
    virtual void SetVolume(float volume) = 0;
    virtual void SetEffect(AudioEffect effect) = 0;
};
```

---

### 1.2 音频采集管线（Audio Capture Pipeline）

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           音频采集管线 (Audio Capture)                           │
└─────────────────────────────────────────────────────────────────────────────────┘

  平台音频输入              硬件抽象层                   预处理阶段
  ┌────────────┐       ┌──────────────┐           ┌──────────────────┐
  │ AudioUnit  │──────▶│ IAudioCapture│──────────▶│ 系统前处理(可选)  │
  │ (macOS/iOS)│       │ Interface    │  PCM数据   │ • AEC 回声消除   │
  │            │       │              │           │ • ANS 噪声抑制   │
  │ DirectSound│──────▶│ • 读取音频    │──────────▶│ • AGC 自动增益   │
  │ (Windows)  │       │ • 格式转换    │           │                  │
  │            │       │ • 回调通知    │           │                  │
  │ AAudio     │──────▶│              │           └────────┬─────────┘
  │ (Android)  │       └──────────────┘                    │
  └────────────┘                                           ▼
                                                        应用层处理
                                           ┌──────────────────────────┐
                                           │ 音频前处理链             │
                                           │ • 重采样(SWR)           │
                                           │ • 音效处理               │
                                           │ • AI降噪(外部)          │
                                           │ • 频谱分析               │
                                           └──────────┬─────────────┘
                                                      │
                          编码阶段                      ▼
                   ┌─────────────────┐    ┌──────────────────┐
                   │ FFmpeg          │◀───│ 音频帧队列        │
                   │ Audio Encoder   │    │ (环形缓冲区)      │
                   │ (AAC/Opus/      │    │                  │
                   │  PCM)           │    │ • 缓冲控制       │
                   └────────┬────────┘    │ • 丢帧策略       │
                            │             └──────────────────┘
                            ▼
                   ┌─────────────────┐
                   │ 编码后数据       │
                   │ (AAC/Opus)     │
                   └────────┬────────┘
                            │
         ┌──────────────────┼──────────────────┐
         │                  │                  │
         ▼                  ▼                  ▼
  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐
  │ 本地录制      │  │ 网络推流      │  │ 旁路回调      │
  │ (Muxer)     │  │ (RTMP/WebRTC)│  │ (DataBypass) │
  └─────────────┘  └──────────────┘  └──────────────┘
```

#### 音频采集流程说明

| 阶段 | 组件 | 功能 | 关键配置 |
|------|------|------|----------|
| **采集** | IAudioCapture 实现 | 平台特定音频采集 | 采样率、声道数、缓冲区大小 |
| **系统前处理** | Platform 3A | 系统级回声/噪声/增益控制 | enableAEC/ANS/AGC |
| **应用处理** | Audio Processor Chain | 重采样、音效、AI降噪 | effect type, sample rate |
| **缓冲** | AudioFrameQueue | 平滑抖动，控制延迟 | buffer duration (ms) |
| **编码** | FFmpeg Audio Encoder | AAC/Opus/PCM 编码 | bitrate, profile |
| **输出** | Muxer/Network | 文件存储或网络传输 | format, encryption |

#### 关键接口

```cpp
// 音频采集核心接口
class IAudioCapture {
public:
    virtual ErrorCode Initialize(const AudioCaptureConfig& config) = 0;
    virtual ErrorCode Start() = 0;
    virtual ErrorCode Stop() = 0;
    virtual void SetCallback(AudioCaptureCallback callback) = 0;
    virtual ~IAudioCapture() = default;
};

// 音频采集配置
struct AudioCaptureConfig {
    int sampleRate;           // 采样率: 16000/44100/48000
    int channels;             // 声道数: 1/2
    int bitsPerSample;        // 位深: 16
    int bufferDurationMs;     // 缓冲区时长
    bool enableSystem3A;      // 启用系统3A
    bool enableEchoCancellation;
    bool enableNoiseSuppression;
    bool enableAutoGainControl;
};
```

---

## 2. 视频数据处理管线

### 2.1 视频播放管线（Video Playback Pipeline）

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           视频播放管线 (Video Playback)                           │
└─────────────────────────────────────────────────────────────────────────────────┘

  网络/本地源                    解封装                      解码阶段
  ┌─────────┐                ┌──────────────┐           ┌──────────────────┐
  │ HTTP/   │                │ Demuxer      │           │ Video Decoder    │
  │ HLS/    │───────────────▶│ (FFmpeg)     │──────────▶│ • Hardware:      │
  │ File    │   封装数据      │ • 解析容器    │  视频包    │   VideoToolbox  │
  │ RTMP    │                │ • 提取流      │           │   MediaCodec    │
  └─────────┘                │ • 时间戳      │           │   DXVA/D3D11VA  │
                             └──────────────┘           │ • Software:      │
                                                        │   FFmpeg SW     │
                                                        └────────┬─────────┘
                                                                 │
                                    硬件加速优先，自动回退          │
                                                                 ▼
  平台视频输出              渲染器抽象层                 视频处理阶段
  ┌────────────┐       ┌──────────────┐           ┌──────────────────┐
  │ Metal      │◀──────│ IRenderer    │◀─────────│ Video Processor  │
  │ (macOS/iOS)│       │ Interface    │  CVPixel  │ • 格式转换        │
  │            │       │              │  Buffer/│ • 缩放            │
  │ OpenGL ES  │◀──────│ • Zero-copy  │  Texture │ • 滤镜            │
  │ (Android)  │       │   渲染路径    │           │ • 字幕叠加        │
  │            │       │ • 格式适配    │           │                  │
  │ DirectX    │◀──────│ • 同步显示    │           │                  │
  │ (Windows)  │       └──────────────┘           └──────────────────┘
  └────────────┘

  旁路输出 (Phase 4)
  ┌─────────────────────────────────────────────────────────────┐
  │ DataBypassManager                                           │
  │ • onVideoPacketDemuxed()    - 解封装后原始包                 │
  │ • onVideoFrameDecoded()     - 解码后YUV/RGB帧               │
  │ • onVideoFrameProcessed()   - 处理后帧                      │
  │ • onVideoFrameRendered()    - 渲染前帧                      │
  └─────────────────────────────────────────────────────────────┘
```

#### 视频播放流程说明

| 阶段 | 组件 | 功能 | 输出格式 |
|------|------|------|----------|
| **输入源** | NetworkDemuxer/Demuxer | 从网络或本地读取封装数据 | 原始封装数据 |
| **解封装** | FFmpeg Demuxer | 解析容器格式，提取视频包 | AnnexB/AVCC NALU |
| **解码** | Hardware Decoder → SW Decoder | 硬件优先，自动回退软件解码 | CVPixelBuffer/AVFrame |
| **处理** | Video Processor | 格式转换、缩放、滤镜、字幕 | 目标格式 RGB/YUV |
| **渲染** | IRenderer 实现 | Zero-copy 或传统渲染路径 | 屏幕显示 |
| **旁路** | DataBypassManager | 多级数据回调 | 原始包/解码帧/处理后帧 |

#### 硬件解码 Zero-Copy 路径

```
┌────────────────────────────────────────────────────────┐
│                   Zero-Copy 渲染路径                    │
└────────────────────────────────────────────────────────┘

  硬件解码器                  渲染器
  ┌─────────────┐          ┌──────────────┐
  │ VideoToolbox│          │ Metal        │
  │             │          │ Renderer     │
  │ 输出:        │          │              │
  │ CVPixelBuffer│─────────▶│ 输入:        │
  │ (IOSurface) │  共享内存  │ CVPixelBuffer │
  │             │  无CPU拷贝 │ (纹理包装)    │
  └─────────────┘          └──────────────┘

  传统路径 (有拷贝)
  ┌─────────────┐          ┌──────────────┐          ┌──────────────┐
  │ SW Decoder  │─────────▶│ CPU内存拷贝   │─────────▶│ GPU纹理上传   │
  │ AVFrame     │  YUV数据  │              │  RGB数据  │              │
  │ (GPU内存)   │          │              │          │ 渲染显示      │
  └─────────────┘          └──────────────┘          └──────────────┘
```

#### 关键接口

```cpp
// 视频渲染核心接口
class IRenderer {
public:
    virtual ErrorCode Initialize(void* nativeWindow, int width, int height) = 0;
    virtual ErrorCode RenderFrame(const AVFrame* frame) = 0;
    virtual ErrorCode RenderCVPixelBuffer(void* cvPixelBuffer) = 0;  // Zero-copy
    virtual void Release() = 0;
    virtual ~IRenderer() = default;
};

// 硬件解码器接口
class IHardwareDecoder {
public:
    virtual ErrorCode Initialize(const AVCodecParameters* codecpar) = 0;
    virtual AVFramePtr Decode(const AVPacketPtr& packet) = 0;
    virtual bool IsSupported(AVCodecID codecId) = 0;
    virtual ~IHardwareDecoder() = default;
};

// 视频处理器
class IVideoProcessor {
public:
    virtual VideoFrame Process(const VideoFrame& input) = 0;
    virtual void SetScaleMode(ScaleMode mode) = 0;
    virtual void ApplyFilter(VideoFilter filter) = 0;
};
```

---

### 2.2 视频采集/编码管线（Video Capture/Encoding Pipeline）

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                       视频采集/编码管线 (Video Capture/Encode)                   │
└─────────────────────────────────────────────────────────────────────────────────┘

  视频源                    预处理                        编码阶段
  ┌─────────┐            ┌──────────────┐           ┌──────────────────┐
  │ Camera  │────────────▶│ Frame        │──────────▶│ Hardware Encoder │
  │ Screen  │   原始帧    │ Preprocessor │  处理后帧  │ • VideoToolbox   │
  │ Custom  │            │              │           │ • MediaCodec     │
  │ Buffer  │            │ • 格式转换    │           │ • MediaFoundation│
  └─────────┘            │ • 裁剪        │           │                  │
                         │ • 缩放        │           │ Software Encoder │
                         │ • 旋转        │           │ • FFmpeg x264    │
                         │ • 镜像        │           │ • FFmpeg x265    │
                         │ • 水印        │           │ • FFmpeg VP8/9   │
                         │ • 美颜滤镜    │           └────────┬─────────┘
                         └──────────────┘                    │
                                                              │
                         ┌────────────────────────────────────┼──────────────────┐
                         │                                    │                  │
                         ▼                                    ▼                  ▼
                  ┌─────────────┐                     ┌──────────────┐  ┌──────────────┐
                  │ 编码后数据   │                     │ 本地录制      │  │ 网络推流      │
                  │ (H264/H265) │                     │ (Muxer)     │  │ (RTMP/RTP)   │
                  └──────┬──────┘                     └──────────────┘  └──────────────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
         ▼               ▼               ▼
  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
  │ 旁路回调      │ │ 截图          │ │ 预览渲染      │
  │ DataBypass   │ │ Screenshot   │ │ Preview      │
  └──────────────┘ └──────────────┘ └──────────────┘
```

#### 视频采集/编码流程说明

| 阶段 | 组件 | 功能 | 关键配置 |
|------|------|------|----------|
| **采集源** | Camera/Screen/Custom | 获取原始视频帧 | resolution, fps |
| **预处理** | FramePreprocessor | 格式转换、裁剪、缩放、旋转、水印 | crop, scale, rotate |
| **编码** | Hardware → Software | 硬件编码优先，自动回退 | codec, bitrate, profile |
| **后处理** | Muxer/Streamer | 封装或网络传输 | container, encryption |
| **旁路** | DataBypassManager | 原始帧/编码帧回调 | callback frequency |

#### 关键接口

```cpp
// 视频采集接口
class IVideoCapture {
public:
    virtual ErrorCode Initialize(const VideoCaptureConfig& config) = 0;
    virtual ErrorCode Start() = 0;
    virtual ErrorCode Stop() = 0;
    virtual void SetCallback(VideoCaptureCallback callback) = 0;
    virtual ~IVideoCapture() = default;
};

// 视频编码器接口
class IVideoEncoder {
public:
    virtual ErrorCode Initialize(const VideoEncoderConfig& config) = 0;
    virtual EncodedFrame Encode(const VideoFrame& frame, bool forceKeyFrame) = 0;
    virtual void RequestKeyFrame() = 0;
    virtual ~IVideoEncoder() = default;
};

// 编码器配置
struct VideoEncoderConfig {
    VideoCodecType codec;       // H264/H265/VP8/VP9
    int width;
    int height;
    int frameRate;
    int bitrate;                // bps
    int keyFrameInterval;       // 关键帧间隔(秒)
    bool enableHardware;        // 优先硬件编码
    int profile;                // 编码profile
    int level;                // 编码level
};
```

---

## 3. 数据旁路机制（Data Bypass）

### 3.1 旁路回调点

```
┌─────────────────────────────────────────────────────────────────┐
│                      数据旁路回调点                              │
└─────────────────────────────────────────────────────────────────┘

  音频管线                    视频管线
  ┌─────────┐                ┌─────────┐
  │ Demuxer │──────┬────────▶│ Demuxer │──────┬────────▶ onPacketDemuxed()
  └────┬────┘      │        └────┬────┘      │
       │           │             │           │
       ▼           │             ▼           │
  ┌─────────┐      │        ┌─────────┐      │
  │ Decoder │──────┼───────▶│ Decoder │──────┼────────▶ onFrameDecoded()
  └────┬────┘      │        └────┬────┘      │
       │           │             │           │
       ▼           │             ▼           │
  ┌─────────┐      │        ┌─────────┐      │
  │ Process │──────┼───────▶│ Process │──────┼────────▶ onFrameProcessed()
  └────┬────┘      │        └────┬────┘      │
       │           │             │           │
       ▼           │             ▼           │
  ┌─────────┐      │        ┌─────────┐      │
  │ Render  │──────┘        │ Render  │──────┘────────▶ onFrameRendered()
  └─────────┘               └─────────┘

  编码管线
  ┌─────────┐
  │ Capture │──────────────────────▶ onRawFrameCaptured()
  └────┬────┘
       │
       ▼
  ┌─────────┐
  │ Encoder │──────────────────────▶ onEncodedFrame()
  └────┬────┘
       │
       ▼
  ┌─────────┐
  │ Muxer   │──────────────────────▶ onMuxedData()
  └─────────┘
```

### 3.2 旁路回调接口

```cpp
// 数据旁路回调接口
class IDataBypass {
public:
    // 音频旁路
    virtual void onAudioPacketDemuxed(const AudioPacket& packet) = 0;
    virtual void onAudioFrameDecoded(const AudioFrame& frame) = 0;
    virtual void onAudioFrameProcessed(const AudioFrame& frame) = 0;
    virtual void onAudioFrameRendered(const AudioFrame& frame) = 0;

    // 视频旁路
    virtual void onVideoPacketDemuxed(const VideoPacket& packet) = 0;
    virtual void onVideoFrameDecoded(const VideoFrame& frame) = 0;
    virtual void onVideoFrameProcessed(const VideoFrame& frame) = 0;
    virtual void onVideoFrameRendered(const VideoFrame& frame) = 0;

    // 采集/编码旁路
    virtual void onRawFrameCaptured(const VideoFrame& frame) = 0;
    virtual void onEncodedFrame(const EncodedFrame& frame) = 0;
    virtual void onMuxedData(const uint8_t* data, size_t size) = 0;
};

// 旁路管理器
class DataBypassManager {
public:
    void RegisterBypass(std::shared_ptr<IDataBypass> bypass);
    void UnregisterBypass(std::shared_ptr<IDataBypass> bypass);

    // 分发数据到所有注册的旁路
    void DispatchAudioFrame(const AudioFrame& frame, BypassStage stage);
    void DispatchVideoFrame(const VideoFrame& frame, BypassStage stage);
    void DispatchEncodedFrame(const EncodedFrame& frame);
};

enum class BypassStage {
    Demuxed,    // 解封装后
    Decoded,    // 解码后
    Processed,  // 处理后
    Rendered    // 渲染前/后
};
```

---

## 4. 时钟同步机制

### 4.1 AV 同步架构

```
┌─────────────────────────────────────────────────────────────────┐
│                     AV 时钟同步架构                             │
└─────────────────────────────────────────────────────────────────┘

  音频时钟 (Master)              视频时钟 (Slave)
  ┌───────────────┐              ┌───────────────┐
  │ AudioClock    │              │ VideoClock    │
  │               │              │               │
  │ • 基于样本计数 │              │ • 基于PTS      │
  │ • 高精度计时   │              │ • 校准到音频   │
  │ • 漂移检测    │              │ • 帧调度       │
  └───────┬───────┘              └───────┬───────┘
          │                              │
          │         ┌──────────┐         │
          └────────▶│ Sync     │◀────────┘
                    │ Manager  │
                    │          │
                    │ • 计算漂移│
                    │ • 调整速度│
                    │ • 丢帧决策│
                    └────┬─────┘
                         │
          ┌──────────────┼──────────────┐
          │              │              │
          ▼              ▼              ▼
    ┌──────────┐   ┌──────────┐   ┌──────────┐
    │ 正常播放  │   │ 加速播放  │   │ 丢帧追赶  │
    │          │   │ (慢于音频)│   │ (快于音频)│
    └──────────┘   └──────────┘   └──────────┘
```

### 4.2 同步策略

| 场景 | 策略 | 实现 |
|------|------|------|
| 视频慢于音频 | 加速播放或跳过重复帧 | reduce sleep time |
| 视频快于音频 | 等待或丢帧 | add delay / drop frame |
| 音频慢于视频 | 音频追赶（较少见） | audio resample |
| 初始启动 | 预缓冲音频，等待同步 | audio prebuffer |

---

## 5. 线程模型

### 5.1 播放管线线程

```
┌─────────────────────────────────────────────────────────────────┐
│                        播放管线线程模型                           │
└─────────────────────────────────────────────────────────────────┘

  主线程 (API)              解码线程                  渲染线程
  ┌──────────┐             ┌──────────┐             ┌──────────┐
  │ Play()   │────────────▶│ Video    │             │ Video    │
  │ Pause()  │             │ Decode   │────────────▶│ Render   │
  │ Seek()   │────────────▶│          │   Frame     │          │
  │ Stop()   │             │ Audio    │   Queue     │ Audio    │
  └──────────┘             │ Decode   │────────────▶│ Render   │
                           └──────────┘             └──────────┘
                                │
                                │
                           ┌────▼────┐
                           │ Demuxer │
                           │ Thread  │
                           └─────────┘

  回调线程 (可选)
  ┌──────────┐
  │ Bypass   │
  │ Callback │
  │ Thread   │
  └──────────┘
```

### 5.2 采集管线线程

```
┌─────────────────────────────────────────────────────────────────┐
│                        采集管线线程模型                           │
└─────────────────────────────────────────────────────────────────┘

  采集回调线程              编码线程                  输出线程
  ┌──────────┐             ┌──────────┐             ┌──────────┐
  │ Camera   │────────────▶│ Hardware │             │ Network  │
  │ Capture  │   Frame     │ Encoder  │────────────▶│ Send     │
  │ Callback │             │          │  Encoded    │          │
  │          │             │ Software │  Packet     │ File     │
  │ Audio    │────────────▶│ Encoder  │────────────▶│ Write    │
  │ Capture  │             │ (Fallback)│            │          │
  └──────────┘             └──────────┘             └──────────┘
```

---

## 6. 平台适配层

### 6.1 平台抽象

```
┌─────────────────────────────────────────────────────────────────┐
│                        平台适配层                                │
└─────────────────────────────────────────────────────────────────┘

                    ┌─────────────────────┐
                    │   Platform Interface  │
                    │   (Abstract)          │
                    └──────────┬──────────┘
                               │
           ┌───────────────────┼───────────────────┐
           │                   │                   │
           ▼                   ▼                   ▼
    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
    │ iOS/macOS    │    │ Android      │    │ Windows      │
    │              │    │              │    │              │
    │ • VideoToolbox│    │ • MediaCodec │    │ • DXVA/D3D11 │
    │ • Metal      │    │ • OpenGL ES  │    │ • DirectX    │
    │ • AudioUnit  │    │ • AAudio     │    │ • WASAPI     │
    │ • CoreVideo  │    │ • MediaExtractor│   • MediaFoundation│
    └──────────────┘    └──────────────┘    └──────────────┘
```

### 6.2 硬件编解码支持矩阵

| 平台 | 硬解 (H264) | 硬解 (H265) | 硬解 (VP8/9) | 硬编 (H264) | 硬编 (H265) |
|------|------------|------------|-------------|------------|------------|
| iOS/macOS | ✅ VideoToolbox | ✅ VideoToolbox | ❌ | ✅ VideoToolbox | ✅ VideoToolbox |
| Android | ✅ MediaCodec | ✅ MediaCodec | ✅ MediaCodec | ✅ MediaCodec | ✅ MediaCodec (API 24+) |
| Windows | ✅ DXVA/D3D11VA | ✅ D3D11VA | ✅ D3D11VA | ✅ MediaFoundation | ✅ MediaFoundation |

---

## 7. 内存管理

### 7.1 缓冲区策略

```
┌─────────────────────────────────────────────────────────────────┐
│                         缓冲区管理                               │
└─────────────────────────────────────────────────────────────────┘

  视频帧池 (GPU)              音频缓冲区 (CPU)              编码缓冲区
  ┌──────────────┐           ┌──────────────┐           ┌──────────────┐
  │ CVPixelBuffer │           │ Ring Buffer  │           │ Packet Queue │
  │ Pool         │           │ (Lock-free)  │           │ (Thread-safe)│
  │              │           │              │           │              │
  │ • 预分配    │           │ • 256KB/8MB  │           │ • 动态增长   │
  │ • 复用      │           │ • 循环写入   │           │ • 水位控制   │
  │ • Zero-copy │           │ • 单读单写   │           │ • 丢帧策略   │
  └──────────────┘           └──────────────┘           └──────────────┘
```

---

## 8. 错误处理与回退

### 8.1 降级策略

```
┌─────────────────────────────────────────────────────────────────┐
│                        降级策略链                                │
└─────────────────────────────────────────────────────────────────┘

  正常路径
  ┌──────────▶ Hardware Decoder ──────────┐
  │                                      │
  │ 失败                                  ▼
  │                              ┌──────────────┐
  │                              │ Software     │
  │                              │ Decoder      │
  │                              │ (FFmpeg)     │
  │                              └──────┬───────┘
  │                                     │
  │                              失败    │
  │                                     ▼
  │                              ┌──────────────┐
  └─────────────────────────────▶│ Error Report │
                                 │ & Notify     │
                                 └──────────────┘
```

---

## 9. 配置与调优

### 9.1 播放器配置

```cpp
struct PlayerConfig {
    // 缓冲区配置
    int videoBufferMs = 1000;           // 视频缓冲区(ms)
    int audioBufferMs = 300;            // 音频缓冲区(ms)
    int networkBufferMs = 5000;         // 网络缓冲区(ms)

    // 解码配置
    bool enableHardwareDecoder = true;  // 启用硬件解码
    bool enableFallbackDecoder = true;  // 硬件失败时回退软件解码

    // 同步配置
    SyncMode syncMode = SyncMode::kAudioMaster;  // 同步模式
    int maxSyncDriftMs = 100;           // 最大允许漂移(ms)

    // 旁路配置
    bool enableVideoBypass = false;     // 启用视频旁路
    bool enableAudioBypass = false;     // 启用音频旁路
    BypassStage bypassStage = BypassStage::kDecoded;  // 旁路回调点
};
```

### 9.2 编码器配置

```cpp
struct EncoderConfig {
    // 基础配置
    VideoCodecType codec = VideoCodecType::kH264;
    int width = 1280;
    int height = 720;
    int frameRate = 30;
    int bitrate = 2000000;              // 2 Mbps

    // 硬件配置
    bool enableHardwareEncoder = true;
    int hardwareProfile = 100;          // High profile
    int hardwareLevel = 40;             // Level 4.0

    // 码率控制
    RateControlMode rcMode = RateControlMode::kCBR;
    int keyFrameInterval = 2;           // 关键帧间隔(秒)
    int bFrames = 0;                    // B帧数量
};
```

---

## 10. 性能目标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 首帧时间(本地) | ≤ 500ms | 冷启动 |
| 首帧时间(网络) | ≤ 2s | 含缓冲 |
| 1080p@30fps CPU | < 30% | iPhone 12 同级 |
| 端到端延迟 | ≤ 3s | RTMP/HLS |
| AV 同步偏差 | ≤ 40ms | 人眼感知阈值 |
| 内存增长 | ≤ 100MB | 播放期间 |

---

## 附录

### A. 数据结构定义

```cpp
// 音频帧
struct AudioFrame {
    uint8_t* data;              // 音频数据
    size_t size;                // 数据大小
    int samples;                // 采样数
    int sampleRate;             // 采样率
    int channels;               // 声道数
    int format;                 // 采样格式(AVSampleFormat)
    int64_t pts;                // 时间戳
};

// 视频帧
struct VideoFrame {
    uint8_t* data[8];           // 平面数据指针(YUV/RGB)
    int linesize[8];            // 行大小
    int width;                  // 宽度
    int height;                 // 高度
    int format;                 // 像素格式(AVPixelFormat)
    int64_t pts;                // 时间戳
    bool keyFrame;              // 是否关键帧
};

// 编码帧
struct EncodedFrame {
    uint8_t* data;              // 编码数据
    size_t size;                // 数据大小
    int64_t pts;                // 时间戳
    int64_t dts;                // 解码时间戳
    bool keyFrame;              // 是否关键帧
    VideoCodecType codec;       // 编码类型
};
```

### B. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-04-22 | 初始版本，定义音频/视频播放和采集管线 |

---

**文档状态**: Draft
**维护者**: HorsAVSDK Team
**最后更新**: 2026-04-22
