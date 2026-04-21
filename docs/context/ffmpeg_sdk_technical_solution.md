# FFmpeg跨平台音视频编解码渲染SDK技术实现方案

## 目录
1. [项目概述](#1-项目概述)
2. [FFmpeg版本和编译配置](#2-ffmpeg版本和编译配置)
3. [硬解码方案对比和选择](#3-硬解码方案对比和选择)
4. [渲染方案对比和选择](#4-渲染方案对比和选择)
5. [关键模块实现思路](#5-关键模块实现思路)
6. [性能优化方案](#6-性能优化方案)
7. [各平台差异处理](#7-各平台差异处理)
8. [关键代码片段](#8-关键代码片段)

---

## 1. 项目概述

### 1.1 项目目标
开发一个基于FFmpeg的跨平台音视频编解码渲染播放SDK，支持iOS（含iPadOS）、Android、macOS、Windows四个平台。

### 1.2 技术栈
- **底层**: C++11/14
- **视频编解码**: FFmpeg + 平台硬解码
- **音频编解码**: FFmpeg + 平台音频API
- **渲染**: OpenGL ES / Metal / DirectX
- **网络协议**: FFmpeg内置协议支持

### 1.3 实现阶段
| 阶段 | 功能 | 优先级 |
|------|------|--------|
| 阶段一 | 本地播放（文件解码渲染） | P0 |
| 阶段二 | 网络流播放（RTMP/HLS/HTTP） | P0 |
| 阶段三 | 实时编码（视频录制） | P1 |
| 阶段四 | 数据旁路回调（原始数据输出） | P1 |

---

## 2. FFmpeg版本和编译配置

### 2.1 推荐FFmpeg版本

#### 版本选择: FFmpeg 6.1.x LTS

**选择理由:**
1. **稳定性**: 6.x系列是长期支持版本，bug修复及时
2. **H265支持**: 原生支持HEVC/H265解码，编码通过x265
3. **硬解码完善**: VideoToolbox、MediaCodec、DXVA/D3D11VA支持成熟
4. **API稳定**: 相比7.x，6.x API更稳定，兼容性更好
5. **生态成熟**: 社区文档丰富，第三方库兼容性好

```
版本对比:
- FFmpeg 5.x: 基础功能稳定，但H265硬解支持有限
- FFmpeg 6.x: ★ 推荐，H265硬解完善，API稳定
- FFmpeg 7.x: 新特性多，但API变动大，稳定性待验证
```

### 2.2 编译配置选项

#### 通用配置（所有平台）

```bash
# 基础配置
--enable-shared              # 编译动态库
--enable-static              # 编译静态库
--enable-pic                 # 位置无关代码
--enable-gpl                 # GPL协议（x265需要）
--enable-version3            # LGPL v3

# 优化配置
--enable-optimizations       # 编译优化
--enable-hardcoded-tables    # 硬编码查找表
--disable-debug              # 禁用调试符号（Release）
--enable-debug               # 启用调试符号（Debug）

# 核心组件
--enable-avcodec             # 编解码器
--enable-avformat            # 格式处理
--enable-avutil              # 工具库
--enable-avfilter            # 滤镜
--enable-swresample          # 音频重采样
--enable-swscale             # 视频缩放

# 协议支持
--enable-protocols
--enable-network
--enable-openssl             # HTTPS/TLS支持

# 格式支持
--enable-demuxers
--enable-muxers
--enable-parsers

# 编解码器
--enable-decoders
--enable-encoders
--enable-libx265             # H265编码
--enable-libx264             # H264编码
```

#### 平台特定配置

**macOS/iOS (VideoToolbox)**
```bash
--enable-videotoolbox        # Apple硬解码
--enable-audiotoolbox        # Apple音频编解码
--enable-encoder=h264_videotoolbox
--enable-encoder=hevc_videotoolbox
--target-os=darwin
--arch=arm64/x86_64
```

**Android (MediaCodec)**
```bash
--enable-mediacodec          # Android硬解码
--enable-jni                 # JNI支持
--enable-encoder=h264_mediacodec
--enable-encoder=hevc_mediacodec
--target-os=android
--arch=arm64-v8a/armeabi-v7a/x86_64
--sysroot=$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-x86_64/sysroot
--cross-prefix=$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/
```

**Windows (DXVA/D3D11VA)**
```bash
--enable-dxva2               # DirectX VA 2.0
--enable-d3d11va             # Direct3D 11 VA
--enable-encoder=h264_nvenc  # NVIDIA硬编码（可选）
--enable-encoder=hevc_nvenc
--target-os=mingw32
--arch=x86_64
--cross-prefix=x86_64-w64-mingw32-
```

### 2.3 依赖库选择

| 库名称 | 版本 | 用途 | 必需 |
|--------|------|------|------|
| x265 | 3.5+ | H265软件编码 | 是 |
| x264 | 164+ | H264软件编码 | 是 |
| openssl | 3.0+ | HTTPS/TLS | 是 |
| fdk-aac | 2.0+ | AAC音频编码 | 推荐 |
| libvpx | 1.13+ | VP8/VP9编解码 | 可选 |
| dav1d | 1.0+ | AV1解码 | 可选 |

### 2.4 各平台编译脚本

#### macOS编译脚本

```bash
#!/bin/bash
# build_ffmpeg_macos.sh

FFMPEG_VERSION="6.1.1"
BUILD_DIR="build/macos"
INSTALL_DIR="$(pwd)/install/macos"

# 下载FFmpeg
if [ ! -d "ffmpeg-${FFMPEG_VERSION}" ]; then
    wget https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz
    tar -xf ffmpeg-${FFMPEG_VERSION}.tar.xz
fi

cd ffmpeg-${FFMPEG_VERSION}

# 配置选项
./configure \
    --prefix=${INSTALL_DIR} \
    --enable-shared \
    --enable-static \
    --enable-pic \
    --enable-gpl \
    --enable-version3 \
    --enable-nonfree \
    --enable-libx264 \
    --enable-libx265 \
    --enable-videotoolbox \
    --enable-audiotoolbox \
    --enable-openssl \
    --enable-libfdk-aac \
    --enable-encoder=h264_videotoolbox \
    --enable-encoder=hevc_videotoolbox \
    --enable-encoder=libx264 \
    --enable-encoder=libx265 \
    --enable-encoder=libfdk_aac \
    --enable-decoder=h264 \
    --enable-decoder=hevc \
    --enable-decoder=h264_videotoolbox \
    --enable-decoder=hevc_videotoolbox \
    --enable-protocol=https \
    --enable-demuxer=flv \
    --enable-demuxer=hls \
    --enable-demuxer=rtmp \
    --enable-muxer=mp4 \
    --enable-muxer=flv \
    --disable-doc \
    --disable-programs \
    --arch=arm64 \
    --target-os=darwin \
    --extra-cflags="-I/usr/local/include -arch arm64" \
    --extra-ldflags="-L/usr/local/lib -arch arm64"

# 编译
make -j$(sysctl -n hw.ncpu)
make install
```

#### iOS编译脚本（通用框架）

```bash
#!/bin/bash
# build_ffmpeg_ios.sh

FFMPEG_VERSION="6.1.1"
IOS_DEPLOYMENT_TARGET="12.0"

# 架构列表
ARCHS="arm64 x86_64"

for ARCH in $ARCHS; do
    BUILD_DIR="build/ios/${ARCH}"
    
    if [ "$ARCH" = "arm64" ]; then
        PLATFORM="iphoneos"
        HOST="aarch64-apple-darwin"
    else
        PLATFORM="iphonesimulator"
        HOST="x86_64-apple-darwin"
    fi
    
    SDK_PATH=$(xcrun -sdk $PLATFORM --show-sdk-path)
    
    ./configure \
        --prefix=${BUILD_DIR} \
        --enable-cross-compile \
        --arch=${ARCH} \
        --target-os=darwin \
        --cc="xcrun -sdk ${PLATFORM} clang" \
        --sysroot=${SDK_PATH} \
        --extra-cflags="-arch ${ARCH} -mios-version-min=${IOS_DEPLOYMENT_TARGET}" \
        --extra-ldflags="-arch ${ARCH} -mios-version-min=${IOS_DEPLOYMENT_TARGET}" \
        --enable-pic \
        --enable-videotoolbox \
        --enable-audiotoolbox \
        --enable-libx264 \
        --enable-libx265 \
        --enable-gpl \
        --enable-version3 \
        --disable-programs \
        --disable-doc \
        --disable-ffmpeg \
        --disable-ffplay \
        --disable-ffprobe
    
    make -j$(sysctl -n hw.ncpu)
    make install
done

# 创建通用框架
lipo -create build/ios/arm64/lib/libavcodec.a build/ios/x86_64/lib/libavcodec.a \
    -output install/ios/libavcodec.a
# ... 其他库
```

#### Android编译脚本

```bash
#!/bin/bash
# build_ffmpeg_android.sh

FFMPEG_VERSION="6.1.1"
ANDROID_NDK="/path/to/android-ndk"
API_LEVEL=24

# 架构配置
ARCHS="arm64-v8a armeabi-v7a x86_64"

for ARCH in $ARCHS; do
    case $ARCH in
        arm64-v8a)
            TRIPLE="aarch64-linux-android"
            CPU="armv8-a"
            ;;
        armeabi-v7a)
            TRIPLE="arm-linux-androideabi"
            CPU="armv7-a"
            ;;
        x86_64)
            TRIPLE="x86_64-linux-android"
            CPU="x86-64"
            ;;
    esac
    
    TOOLCHAIN=${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64
    SYSROOT=${TOOLCHAIN}/sysroot
    
    ./configure \
        --prefix=build/android/${ARCH} \
        --enable-cross-compile \
        --target-os=android \
        --arch=${ARCH} \
        --cpu=${CPU} \
        --cc=${TOOLCHAIN}/bin/${TRIPLE}${API_LEVEL}-clang \
        --cxx=${TOOLCHAIN}/bin/${TRIPLE}${API_LEVEL}-clang++ \
        --sysroot=${SYSROOT} \
        --extra-cflags="-O3 -fPIC" \
        --extra-ldflags="-Wl,--no-undefined" \
        --enable-jni \
        --enable-mediacodec \
        --enable-libx264 \
        --enable-libx265 \
        --enable-gpl \
        --enable-version3 \
        --disable-programs \
        --disable-doc
    
    make -j$(nproc)
    make install
done
```

#### Windows编译脚本（MinGW交叉编译）

```bash
#!/bin/bash
# build_ffmpeg_windows.sh

FFMPEG_VERSION="6.1.1"
CROSS_PREFIX="x86_64-w64-mingw32-"

./configure \
    --prefix=build/windows \
    --enable-cross-compile \
    --target-os=mingw32 \
    --arch=x86_64 \
    --cross-prefix=${CROSS_PREFIX} \
    --enable-dxva2 \
    --enable-d3d11va \
    --enable-libx264 \
    --enable-libx265 \
    --enable-gpl \
    --enable-version3 \
    --disable-programs \
    --disable-doc \
    --extra-cflags="-I/usr/local/mingw/include" \
    --extra-ldflags="-L/usr/local/mingw/lib"

make -j$(nproc)
make install
```

---

## 3. 硬解码方案对比和选择

### 3.1 硬解码方案总览

| 平台 | 硬解码API | 支持格式 | 性能 | 兼容性 |
|------|-----------|----------|------|--------|
| iOS/macOS | VideoToolbox | H264/H265/ProRes | ★★★★★ | ★★★★★ |
| Android | MediaCodec | H264/H265/VP8/VP9 | ★★★★☆ | ★★★☆☆ |
| Windows | D3D11VA | H264/H265/VP9 | ★★★★★ | ★★★★☆ |
| Windows | DXVA2 | H264/H265 | ★★★★☆ | ★★★★★ |

### 3.2 iOS/macOS - VideoToolbox详解

#### VideoToolbox架构

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
├─────────────────────────────────────────────────────────┤
│                  VideoToolbox API                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ VTDecompress │  │ VTCompress   │  │ VTSession    │  │
│  │ Session      │  │ Session      │  │ Properties   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────┤
│              CoreMedia / CoreVideo                       │
│         (CVPixelBuffer / CMBlockBuffer)                  │
├─────────────────────────────────────────────────────────┤
│              Hardware Video Decoder                      │
│              (Apple Silicon / Intel)                     │
└─────────────────────────────────────────────────────────┘
```

#### VideoToolbox硬解码初始化

```cpp
// VideoToolbox硬解码器封装
class VideoToolboxDecoder {
public:
    VideoToolboxDecoder();
    ~VideoToolboxDecoder();
    
    bool Initialize(AVCodecContext* codec_ctx);
    int DecodePacket(AVPacket* packet, AVFrame* frame);
    void Flush();
    void Release();
    
private:
    VTDecompressionSessionRef session_;
    CMVideoFormatDescriptionRef format_desc_;
    OSType pixel_format_;
    
    // 回调函数
    static void OutputCallback(void* decompression_output_ref_con,
                               void* source_frame_ref_con,
                               OSStatus status,
                               VTDecodeInfoFlags info_flags,
                               CVImageBufferRef image_buffer,
                               CMTime presentation_time_stamp,
                               CMTime presentation_duration);
    
    bool CreateSession();
    bool CreateFormatDescription(AVCodecContext* ctx);
    CVPixelBufferRef ConvertToPixelBuffer(AVFrame* frame);
};
```

#### VideoToolbox关键配置

```cpp
bool VideoToolboxDecoder::Initialize(AVCodecContext* codec_ctx) {
    // 1. 创建格式描述
    if (!CreateFormatDescription(codec_ctx)) {
        return false;
    }
    
    // 2. 配置解码器属性
    CFDictionaryRef decoder_spec = nullptr;
    CFMutableDictionaryRef output_callback = nullptr;
    
    // 硬件加速要求
    const void* keys[] = { kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder };
    const void* values[] = { kCFBooleanTrue };
    decoder_spec = CFDictionaryCreate(nullptr, keys, values, 1, 
                                       &kCFTypeDictionaryKeyCallBacks,
                                       &kCFTypeDictionaryValueCallBacks);
    
    // 3. 设置输出像素格式
    OSType pixel_formats[] = { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                               kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
                               kCVPixelFormatType_32BGRA };
    CFArrayRef pixel_format_array = CFArrayCreate(nullptr, 
        reinterpret_cast<const void**>(pixel_formats), 3, nullptr);
    
    CFMutableDictionaryRef output_image_buffer_attrs = 
        CFDictionaryCreateMutable(nullptr, 0, 
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(output_image_buffer_attrs, 
        kCVPixelBufferPixelFormatTypeKey, pixel_format_array);
    
    // 4. 创建解码会话
    VTDecompressionOutputCallbackRecord callback_record;
    callback_record.decompressionOutputCallback = OutputCallback;
    callback_record.decompressionOutputRefCon = this;
    
    OSStatus status = VTDecompressionSessionCreate(
        nullptr,                    // allocator
        format_desc_,               // video format description
        decoder_spec,               // video decoder specification
        output_image_buffer_attrs,  // destination image buffer attributes
        &callback_record,           // output callback
        &session_                   // decompression session out
    );
    
    // 清理
    CFRelease(decoder_spec);
    CFRelease(pixel_format_array);
    CFRelease(output_image_buffer_attrs);
    
    return status == noErr;
}
```

### 3.3 Android - MediaCodec详解

#### MediaCodec架构

```
┌─────────────────────────────────────────────────────────┐
│                    Java/Kotlin Layer                     │
│                  android.media.MediaCodec                │
├─────────────────────────────────────────────────────────┤
│                    JNI Bridge                            │
├─────────────────────────────────────────────────────────┤
│                  Native MediaCodec                       │
│              (AMediaCodec / NDK API)                     │
├─────────────────────────────────────────────────────────┤
│              OMX / Codec2 HAL                            │
├─────────────────────────────────────────────────────────┤
│              Hardware Codec Driver                       │
│         (Qualcomm / MediaTek / Exynos)                   │
└─────────────────────────────────────────────────────────┘
```

#### MediaCodec硬解码初始化

```cpp
// MediaCodec硬解码器封装
class MediaCodecDecoder {
public:
    MediaCodecDecoder();
    ~MediaCodecDecoder();
    
    bool Initialize(AVCodecContext* codec_ctx, JNIEnv* env);
    int DecodePacket(AVPacket* packet, AVFrame* frame);
    void Flush();
    void Release();
    
private:
    AMediaCodec* codec_;
    AMediaFormat* format_;
    
    // 格式转换
    media_status_t ConfigureFormat(AVCodecContext* ctx);
    bool SendPacket(AVPacket* packet);
    int ReceiveFrame(AVFrame* frame);
    
    // 零拷贝相关
    ANativeWindow* native_window_;
    bool use_surface_;
};
```

#### MediaCodec关键配置

```cpp
bool MediaCodecDecoder::Initialize(AVCodecContext* codec_ctx, JNIEnv* env) {
    // 1. 确定编解码器名称
    const char* mime_type = nullptr;
    if (codec_ctx->codec_id == AV_CODEC_ID_H264) {
        mime_type = "video/avc";
    } else if (codec_ctx->codec_id == AV_CODEC_ID_HEVC) {
        mime_type = "video/hevc";
    } else {
        return false;
    }
    
    // 2. 创建MediaCodec
    codec_ = AMediaCodec_createDecoderByType(mime_type);
    if (!codec_) {
        return false;
    }
    
    // 3. 配置格式
    format_ = AMediaFormat_new();
    AMediaFormat_setString(format_, AMEDIAFORMAT_KEY_MIME, mime_type);
    AMediaFormat_setInt32(format_, AMEDIAFORMAT_KEY_WIDTH, codec_ctx->width);
    AMediaFormat_setInt32(format_, AMEDIAFORMAT_KEY_HEIGHT, codec_ctx->height);
    
    // 4. 设置额外数据（SPS/PPS）
    if (codec_ctx->extradata && codec_ctx->extradata_size > 0) {
        AMediaFormat_setBuffer(format_, "csd-0", 
            codec_ctx->extradata, codec_ctx->extradata_size);
    }
    
    // 5. 配置输出Surface（零拷贝渲染）
    if (native_window_) {
        media_status_t status = AMediaCodec_configure(
            codec_, format_, native_window_, nullptr, 0);
    } else {
        media_status_t status = AMediaCodec_configure(
            codec_, format_, nullptr, nullptr, 0);
    }
    
    // 6. 启动解码器
    media_status_t status = AMediaCodec_start(codec_);
    
    return status == AMEDIA_OK;
}
```

### 3.4 Windows - DXVA/D3D11VA详解

#### DXVA2 vs D3D11VA对比

| 特性 | DXVA2 | D3D11VA |
|------|-------|---------|
| DirectX版本 | DX9 | DX11 |
| 性能 | 良好 | 优秀 |
| 延迟 | 较高 | 较低 |
| 多线程 | 有限 | 优秀 |
| H265 10bit | 不支持 | 支持 |
| 推荐度 | ★★★☆☆ | ★★★★★ |

#### D3D11VA架构

```
┌─────────────────────────────────────────────────────────┐
│                  FFmpeg D3D11VA                         │
│              (hwaccel + hwdevice)                        │
├─────────────────────────────────────────────────────────┤
│              D3D11 Video API                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ ID3D11Device │  │ID3D11Video   │  │ID3D11Video   │  │
│  │              │  │Device        │  │Context       │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────┤
│              D3D11 Video Decoder                         │
├─────────────────────────────────────────────────────────┤
│              GPU Driver (WDDM)                           │
└─────────────────────────────────────────────────────────┘
```

#### D3D11VA硬解码初始化

```cpp
// D3D11VA硬解码器封装
class D3D11VADecoder {
public:
    D3D11VADecoder();
    ~D3D11VADecoder();
    
    bool Initialize(AVCodecContext* codec_ctx);
    int DecodePacket(AVPacket* packet, AVFrame* frame);
    void Flush();
    void Release();
    
    // 获取纹理用于渲染
    ID3D11Texture2D* GetTexture(AVFrame* frame);
    
private:
    AVBufferRef* hw_device_ctx_;
    AVBufferRef* hw_frames_ctx_;
    
    // D3D11对象
    ID3D11Device* d3d11_device_;
    ID3D11DeviceContext* d3d11_context_;
    ID3D11VideoDevice* video_device_;
    ID3D11VideoContext* video_context_;
    
    bool CreateDevice();
    bool CreateHWDeviceContext();
    bool InitHWFramesContext(AVCodecContext* ctx);
};
```

#### D3D11VA关键配置

```cpp
bool D3D11VADecoder::Initialize(AVCodecContext* codec_ctx) {
    // 1. 创建D3D11设备
    if (!CreateDevice()) {
        return false;
    }
    
    // 2. 创建FFmpeg硬件设备上下文
    if (!CreateHWDeviceContext()) {
        return false;
    }
    
    // 3. 设置硬件设备上下文到解码器
    codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
    
    // 4. 配置硬件加速
    codec_ctx->get_format = GetHWFormat;
    
    // 5. 初始化硬件帧上下文
    if (!InitHWFramesContext(codec_ctx)) {
        return false;
    }
    
    return true;
}

AVPixelFormat D3D11VADecoder::GetHWFormat(AVCodecContext* ctx, 
                                          const AVPixelFormat* pix_fmts) {
    for (const AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == AV_PIX_FMT_D3D11) {
            return *p;
        }
    }
    return AV_PIX_FMT_NONE;
}

bool D3D11VADecoder::CreateDevice() {
    UINT create_device_flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
    
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // adapter
        D3D_DRIVER_TYPE_HARDWARE,   // driver type
        nullptr,                    // software
        create_device_flags,
        feature_levels,             // feature levels
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &d3d11_device_,
        nullptr,
        &d3d11_context_
    );
    
    if (FAILED(hr)) {
        return false;
    }
    
    // 获取视频设备接口
    hr = d3d11_device_->QueryInterface(__uuidof(ID3D11VideoDevice),
                                       (void**)&video_device_);
    hr = d3d11_context_->QueryInterface(__uuidof(ID3D11VideoContext),
                                        (void**)&video_context_);
    
    return SUCCEEDED(hr);
}
```

### 3.5 软硬解码切换策略

```cpp
// 解码器工厂类
class DecoderFactory {
public:
    enum DecoderType {
        DECODER_AUTO,       // 自动选择
        DECODER_HARDWARE,   // 强制硬解
        DECODER_SOFTWARE    // 强制软解
    };
    
    static std::unique_ptr<VideoDecoder> CreateDecoder(
        AVCodecContext* codec_ctx,
        DecoderType preferred_type = DECODER_AUTO) {
        
        // 1. 检查是否支持硬解
        bool hw_supported = CheckHardwareSupport(codec_ctx);
        
        // 2. 根据策略选择
        if (preferred_type == DECODER_HARDWARE && hw_supported) {
            auto decoder = CreateHardwareDecoder(codec_ctx);
            if (decoder && decoder->Initialize(codec_ctx)) {
                return decoder;
            }
        }
        
        if (preferred_type == DECODER_AUTO && hw_supported) {
            auto decoder = CreateHardwareDecoder(codec_ctx);
            if (decoder && decoder->Initialize(codec_ctx)) {
                return decoder;
            }
            // 硬解失败，回退到软解
        }
        
        // 3. 创建软件解码器
        return CreateSoftwareDecoder(codec_ctx);
    }
    
private:
    static bool CheckHardwareSupport(AVCodecContext* codec_ctx) {
        // 检查编解码器是否支持硬解
        const AVCodec* codec = codec_ctx->codec;
        
        for (const AVCodecHWConfig* config = avcodec_get_hw_config(codec, 0);
             config; config = avcodec_get_hw_config(codec, config->index + 1)) {
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
                // 检查设备类型
                switch (config->device_type) {
                    case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
                    case AV_HWDEVICE_TYPE_MEDIACODEC:
                    case AV_HWDEVICE_TYPE_D3D11VA:
                    case AV_HWDEVICE_TYPE_DXVA2:
                        return true;
                    default:
                        break;
                }
            }
        }
        return false;
    }
};
```

### 3.6 硬解码失败回退机制

```cpp
class VideoDecoderWithFallback : public VideoDecoder {
public:
    VideoDecoderWithFallback() : hw_decoder_(nullptr), sw_decoder_(nullptr) {}
    
    bool Initialize(AVCodecContext* codec_ctx) override {
        // 1. 尝试硬解
        hw_decoder_ = CreateHardwareDecoder(codec_ctx);
        if (hw_decoder_ && hw_decoder_->Initialize(codec_ctx)) {
            current_decoder_ = hw_decoder_.get();
            return true;
        }
        
        // 2. 硬解失败，回退到软解
        LOG_WARNING("Hardware decode init failed, fallback to software");
        hw_decoder_.reset();
        
        sw_decoder_ = CreateSoftwareDecoder(codec_ctx);
        if (sw_decoder_ && sw_decoder_->Initialize(codec_ctx)) {
            current_decoder_ = sw_decoder_.get();
            return true;
        }
        
        return false;
    }
    
    int DecodePacket(AVPacket* packet, AVFrame* frame) override {
        int ret = current_decoder_->DecodePacket(packet, frame);
        
        // 运行时硬解失败检测
        if (ret < 0 && current_decoder_ == hw_decoder_.get()) {
            LOG_WARNING("Hardware decode error, switching to software");
            
            // 保存当前状态
            int64_t current_pts = packet->pts;
            
            // 切换到软解
            current_decoder_ = sw_decoder_.get();
            sw_decoder_->Flush();
            
            // 重新发送包
            ret = current_decoder_->DecodePacket(packet, frame);
        }
        
        return ret;
    }
    
private:
    std::unique_ptr<VideoDecoder> hw_decoder_;
    std::unique_ptr<VideoDecoder> sw_decoder_;
    VideoDecoder* current_decoder_;
};
```



---

## 4. 渲染方案对比和选择

### 4.1 渲染方案总览

| 平台 | 渲染API | 性能 | 兼容性 | 维护状态 | 推荐度 |
|------|---------|------|--------|----------|--------|
| iOS | OpenGL ES | ★★★★☆ | ★★★★★ | 已弃用 | ★★☆☆☆ |
| iOS | Metal | ★★★★★ | ★★★★★ | 活跃 | ★★★★★ |
| Android | OpenGL ES 3.0 | ★★★★☆ | ★★★★★ | 活跃 | ★★★★★ |
| Android | Vulkan | ★★★★★ | ★★★☆☆ | 活跃 | ★★★☆☆ |
| macOS | OpenGL | ★★★☆☆ | ★★★★☆ | 已弃用 | ★★☆☆☆ |
| macOS | Metal | ★★★★★ | ★★★★★ | 活跃 | ★★★★★ |
| Windows | DirectX 11 | ★★★★★ | ★★★★★ | 活跃 | ★★★★★ |
| Windows | DirectX 12 | ★★★★★ | ★★★★☆ | 活跃 | ★★★★☆ |

### 4.2 iOS - OpenGL ES vs Metal对比

#### 对比分析

| 特性 | OpenGL ES | Metal |
|------|-----------|-------|
| CPU开销 | 较高 | 低 |
| 内存带宽 | 较高 | 低 |
| 启动时间 | 快 | 快 |
| 学习曲线 | 平缓 | 陡峭 |
| Apple支持 | 已弃用(iOS12后) | 主推 |
| 多线程 | 有限 | 优秀 |
| 调试工具 | 有限 | 强大 |

**结论**: iOS新开发必须选择Metal，OpenGL ES仅用于兼容旧设备。

#### Metal渲染架构

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
├─────────────────────────────────────────────────────────┤
│                    Metal API                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ MTLDevice    │  │ MTLCommand   │  │ MTLRender    │  │
│  │              │  │ Queue        │  │ PipelineState│  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────┤
│              Metal Shaders (MSL)                         │
│         Vertex Shader / Fragment Shader                  │
├─────────────────────────────────────────────────────────┤
│              CAMetalLayer (渲染目标)                      │
├─────────────────────────────────────────────────────────┤
│              GPU (Apple Silicon / A系列)                  │
└─────────────────────────────────────────────────────────┘
```

#### Metal渲染器实现

```cpp
// Metal渲染器封装
class MetalRenderer : public VideoRenderer {
public:
    MetalRenderer();
    ~MetalRenderer();
    
    bool Initialize(void* native_view) override;
    bool RenderFrame(AVFrame* frame) override;
    void Resize(int width, int height) override;
    void Release() override;
    
private:
    // Metal对象
    id<MTLDevice> device_;
    id<MTLCommandQueue> command_queue_;
    id<MTLRenderPipelineState> pipeline_state_;
    CAMetalLayer* metal_layer_;
    
    // 顶点缓冲区
    id<MTLBuffer> vertex_buffer_;
    
    // 纹理
    id<MTLTexture> y_texture_;
    id<MTLTexture> uv_texture_;
    
    // 渲染目标
    id<MTLTexture> render_target_texture_;
    
    // 着色器函数
    id<MTLFunction> vertex_function_;
    id<MTLFunction> fragment_function_;
    
    bool CreateDevice();
    bool CreatePipeline();
    bool CreateTextures(int width, int height);
    bool UploadFrame(AVFrame* frame);
    void Render();
};
```

#### Metal YUV到RGB转换着色器

```metal
// Shaders.metal
#include <metal_stdlib>
using namespace metal;

// 顶点结构
struct VertexIn {
    float2 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
};

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
    float2 uv = uvTexture.sample(textureSampler, in.texCoord).rg;
    float u = uv.r - 0.5;
    float v = uv.g - 0.5;
    
    // BT.601 / BT.709 色彩空间转换
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;
    
    return float4(r, g, b, 1.0);
}

// 片段着色器 - NV12 to RGB
fragment float4 fragmentShaderNV12(VertexOut in [[stage_in]],
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

### 4.3 Android - OpenGL ES详细方案

#### OpenGL ES渲染架构

```
┌─────────────────────────────────────────────────────────┐
│                    Java SurfaceView                      │
│              / TextureView / SurfaceTexture              │
├─────────────────────────────────────────────────────────┤
│                    EGL Context                           │
├─────────────────────────────────────────────────────────┤
│              OpenGL ES 3.0 API                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ glGen*       │  │ glTex*       │  │ glDraw*      │  │
│  │ glCreate*    │  │ glShader*    │  │ glViewport   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────┤
│              GLSL Shaders                                │
├─────────────────────────────────────────────────────────┤
│              GPU Driver (OpenGL ES)                      │
└─────────────────────────────────────────────────────────┘
```

#### OpenGL ES渲染器实现

```cpp
// OpenGL ES渲染器封装
class GLESRenderer : public VideoRenderer {
public:
    GLESRenderer();
    ~GLESRenderer();
    
    bool Initialize(ANativeWindow* native_window) override;
    bool RenderFrame(AVFrame* frame) override;
    void Resize(int width, int height) override;
    void Release() override;
    
private:
    EGLDisplay egl_display_;
    EGLSurface egl_surface_;
    EGLContext egl_context_;
    
    GLuint program_;
    GLuint vertex_buffer_;
    GLuint vao_;
    
    // 纹理
    GLuint y_texture_;
    GLuint u_texture_;
    GLuint v_texture_;
    GLuint uv_texture_;
    
    // Uniform位置
    GLint y_sampler_loc_;
    GLint u_sampler_loc_;
    GLint v_sampler_loc_;
    GLint uv_sampler_loc_;
    
    int video_width_;
    int video_height_;
    AVPixelFormat pixel_format_;
    
    bool InitializeEGL();
    bool CreateShaders();
    bool CreateTextures(int width, int height);
    bool UploadFrame(AVFrame* frame);
    void Render();
};
```

#### OpenGL ES YUV着色器

```glsl
// vertex_shader.glsl
#version 300 es
in vec2 aPosition;
in vec2 aTexCoord;
out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
}

// fragment_shader_yuv420p.glsl
#version 300 es
precision mediump float;
in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D ySampler;
uniform sampler2D uSampler;
uniform sampler2D vSampler;

void main() {
    float y = texture(ySampler, vTexCoord).r;
    float u = texture(uSampler, vTexCoord).r - 0.5;
    float v = texture(vSampler, vTexCoord).r - 0.5;
    
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;
    
    fragColor = vec4(r, g, b, 1.0);
}

// fragment_shader_nv12.glsl
#version 300 es
precision mediump float;
in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D ySampler;
uniform sampler2D uvSampler;

void main() {
    float y = texture(ySampler, vTexCoord).r;
    vec2 uv = texture(uvSampler, vTexCoord).rg - 0.5;
    
    float r = y + 1.402 * uv.y;
    float g = y - 0.344136 * uv.x - 0.714136 * uv.y;
    float b = y + 1.772 * uv.x;
    
    fragColor = vec4(r, g, b, 1.0);
}
```

### 4.4 macOS - OpenGL vs Metal对比

#### 对比分析

| 特性 | OpenGL | Metal |
|------|--------|-------|
| macOS支持 | 10.0+ | 10.11+ |
| 性能 | ★★★☆☆ | ★★★★★ |
| Apple支持 | 已弃用(10.14后) | 主推 |
| 与iOS共享代码 | 否 | 是 |
| 调试工具 | 有限 | 强大 |

**结论**: macOS新开发必须选择Metal，可与iOS共享大部分渲染代码。

### 4.5 Windows - DirectX方案

#### DirectX 11渲染架构

```
┌─────────────────────────────────────────────────────────┐
│                    Win32 Window                          │
├─────────────────────────────────────────────────────────┤
│              DirectX 11 API                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ ID3D11Device │  │ ID3D11Device │  │ IDXGISwap    │  │
│  │              │  │ Context      │  │ Chain        │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────┤
│              HLSL Shaders                                │
├─────────────────────────────────────────────────────────┤
│              GPU Driver (D3D11)                          │
└─────────────────────────────────────────────────────────┘
```

#### DirectX 11渲染器实现

```cpp
// DirectX 11渲染器封装
class D3D11Renderer : public VideoRenderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();
    
    bool Initialize(HWND hwnd) override;
    bool RenderFrame(AVFrame* frame) override;
    void Resize(int width, int height) override;
    void Release() override;
    
private:
    // D3D11设备
    ID3D11Device* device_;
    ID3D11DeviceContext* context_;
    IDXGISwapChain* swap_chain_;
    ID3D11RenderTargetView* render_target_view_;
    
    // 着色器
    ID3D11VertexShader* vertex_shader_;
    ID3D11PixelShader* pixel_shader_;
    ID3D11InputLayout* input_layout_;
    
    // 缓冲区
    ID3D11Buffer* vertex_buffer_;
    ID3D11Buffer* constant_buffer_;
    
    // 纹理
    ID3D11Texture2D* y_texture_;
    ID3D11Texture2D* uv_texture_;
    ID3D11ShaderResourceView* y_srv_;
    ID3D11ShaderResourceView* uv_srv_;
    ID3D11SamplerState* sampler_state_;
    
    HWND hwnd_;
    int width_;
    int height_;
    
    bool InitializeD3D11();
    bool CreateShaders();
    bool CreateTextures(int width, int height);
    bool UploadFrame(AVFrame* frame);
    void Render();
};
```

#### HLSL YUV到RGB转换着色器

```hlsl
// VertexShader.hlsl
struct VertexInput {
    float2 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VertexOutput VS(VertexInput input) {
    VertexOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.texCoord = input.texCoord;
    return output;
}

// PixelShader.hlsl
Texture2D yTexture : register(t0);
Texture2D uvTexture : register(t1);
SamplerState samplerState : register(s0);

float4 PS(VertexOutput input) : SV_TARGET {
    float y = yTexture.Sample(samplerState, input.texCoord).r;
    float2 uv = uvTexture.Sample(samplerState, input.texCoord).rg - 0.5;
    
    float r = y + 1.402 * uv.y;
    float g = y - 0.344136 * uv.x - 0.714136 * uv.y;
    float b = y + 1.772 * uv.x;
    
    return float4(r, g, b, 1.0);
}
```

### 4.6 渲染管线设计

```
┌─────────────────────────────────────────────────────────────────┐
│                      渲染管线架构                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐  │
│  │  AVFrame │───▶│ 格式转换 │───▶│ 纹理上传 │───▶│ 渲染绘制 │  │
│  │ (YUV)    │    │ (如果需要)│    │          │    │          │  │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘  │
│       │                                               │         │
│       │         ┌─────────────────────────────────────┘         │
│       │         ▼                                               │
│       │    ┌──────────┐    ┌──────────┐    ┌──────────┐        │
│       └───▶│ 顶点着色器│───▶│ 片元着色器│───▶│ 帧缓冲区 │        │
│            │          │    │ (YUV-RGB)│    │ (显示)   │        │
│            └──────────┘    └──────────┘    └──────────┘        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 渲染管线基类设计

```cpp
// 渲染管线基类
class RenderPipeline {
public:
    virtual ~RenderPipeline() = default;
    
    // 初始化
    virtual bool Initialize(const RenderConfig& config) = 0;
    
    // 渲染帧
    virtual bool RenderFrame(AVFrame* frame) = 0;
    
    // 调整大小
    virtual void Resize(int width, int height) = 0;
    
    // 设置渲染区域
    virtual void SetRenderRect(const Rect& rect) = 0;
    
    // 清理
    virtual void Release() = 0;
    
    // 获取渲染统计
    virtual RenderStats GetStats() const = 0;
};

// 渲染配置
struct RenderConfig {
    void* native_window;        // 平台相关窗口句柄
    int window_width;           // 窗口宽度
    int window_height;          // 窗口高度
    AVPixelFormat pixel_format; // 输入像素格式
    bool use_hardware_buffer;   // 是否使用硬件缓冲区
    bool enable_vsync;          // 是否启用垂直同步
};

// 渲染统计
struct RenderStats {
    int frames_rendered;        // 已渲染帧数
    int frames_dropped;         // 丢弃帧数
    float average_render_time;  // 平均渲染时间(ms)
    float current_fps;          // 当前FPS
};
```

---

## 5. 关键模块实现思路

### 5.1 SDK整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        SDK Public API                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ MediaPlayer  │  │ MediaRecorder│  │ DataCallback │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
├─────────────────────────────────────────────────────────────────┤
│                      C++ Core Layer                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │ Demuxer  │  │ Decoder  │  │ Renderer │  │ Encoder  │        │
│  │  Module  │  │  Module  │  │  Module  │  │  Module  │        │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘        │
├─────────────────────────────────────────────────────────────────┤
│                      FFmpeg Layer                                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │ avformat │  │ avcodec  │  │ swscale  │  │ swresample│        │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘        │
├─────────────────────────────────────────────────────────────────┤
│                    Platform Layer                                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │VideoToolbox│ │MediaCodec│  │  Metal   │  │ DirectX  │        │
│  │  /Metal   │  │ /GLES    │  │  /GLES   │  │   11     │        │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘        │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Demuxer模块

#### 功能职责
- 解析各种媒体格式（MP4、FLV、MKV、AVI等）
- 支持网络协议（RTMP、HLS、HTTP、DASH等）
- 提取音视频流
- 管理时间戳和同步

#### 类设计

```cpp
// Demuxer模块
class Demuxer {
public:
    Demuxer();
    ~Demuxer();
    
    // 打开媒体源
    bool Open(const std::string& url);
    bool Open(const MediaSource& source);
    
    // 关闭
    void Close();
    
    // 读取数据包
    int ReadPacket(AVPacket* packet);
    
    // 定位
    int Seek(int64_t timestamp, int flags);
    
    // 获取流信息
    int GetVideoStreamIndex() const;
    int GetAudioStreamIndex() const;
    AVStream* GetVideoStream() const;
    AVStream* GetAudioStream() const;
    
    // 获取媒体信息
    MediaInfo GetMediaInfo() const;
    
    // 设置回调
    void SetEventCallback(EventCallback callback);
    
private:
    AVFormatContext* format_ctx_;
    int video_stream_index_;
    int audio_stream_index_;
    
    std::string url_;
    bool is_network_stream_;
    
    // 网络相关
    std::atomic<bool> interrupt_requested_;
    static int InterruptCallback(void* ctx);
    
    // 缓冲控制
    int64_t buffer_duration_;      // 缓冲时长(ms)
    int64_t buffer_size_;          // 缓冲大小(bytes)
    
    // 事件回调
    EventCallback event_callback_;
};

// 媒体信息结构
struct MediaInfo {
    std::string format_name;
    int64_t duration_ms;           // 总时长
    int64_t bitrate;               // 码率
    
    // 视频信息
    bool has_video;
    int video_width;
    int video_height;
    double video_frame_rate;
    std::string video_codec_name;
    
    // 音频信息
    bool has_audio;
    int audio_sample_rate;
    int audio_channels;
    std::string audio_codec_name;
};
```

#### 支持的格式和协议

```cpp
// 支持的格式
const std::vector<std::string> SUPPORTED_FORMATS = {
    "mp4", "mov", "m4v",          // MPEG-4
    "flv", "f4v",                  // Flash Video
    "mkv", "webm",                 // Matroska
    "avi",                         // AVI
    "ts", "m2ts", "mts",           // MPEG-TS
    "3gp", "3g2",                  // 3GPP
};

// 支持的协议
const std::vector<std::string> SUPPORTED_PROTOCOLS = {
    "file",                        // 本地文件
    "http", "https",               // HTTP/HTTPS
    "rtmp", "rtmps",               // RTMP
    "hls", "http_live_streaming",  // HLS
    "dash",                        // DASH
    "udp", "tcp",                  // 网络传输
};
```

### 5.3 Decoder模块

#### 功能职责
- 视频解码（H264/H265/VP8/VP9等）
- 音频解码（AAC/MP3/OPUS等）
- 软硬解码自动切换
- 解码错误恢复

#### 类设计

```cpp
// 解码器基类
class Decoder {
public:
    virtual ~Decoder() = default;
    
    virtual bool Initialize(const AVCodecParameters* params) = 0;
    virtual int SendPacket(AVPacket* packet) = 0;
    virtual int ReceiveFrame(AVFrame* frame) = 0;
    virtual void Flush() = 0;
    virtual void Release() = 0;
    
    // 获取解码器信息
    virtual DecoderInfo GetInfo() const = 0;
};

// 视频解码器
class VideoDecoder : public Decoder {
public:
    VideoDecoder();
    ~VideoDecoder() override;
    
    bool Initialize(const AVCodecParameters* params) override;
    int SendPacket(AVPacket* packet) override;
    int ReceiveFrame(AVFrame* frame) override;
    void Flush() override;
    void Release() override;
    DecoderInfo GetInfo() const override;
    
    // 视频特有
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    AVPixelFormat GetPixelFormat() const { return pixel_format_; }
    double GetFrameRate() const { return frame_rate_; }
    
private:
    AVCodecContext* codec_ctx_;
    const AVCodec* codec_;
    
    // 硬件加速
    std::unique_ptr<HWAccelContext> hw_ctx_;
    bool use_hwaccel_;
    
    // 视频信息
    int width_;
    int height_;
    AVPixelFormat pixel_format_;
    double frame_rate_;
    
    // 解码统计
    int64_t frames_decoded_;
    int64_t frames_dropped_;
    
    bool OpenCodec(const AVCodecParameters* params);
    bool InitHWAccel();
    static enum AVPixelFormat GetFormat(AVCodecContext* ctx, 
                                        const enum AVPixelFormat* pix_fmts);
};

// 音频解码器
class AudioDecoder : public Decoder {
public:
    AudioDecoder();
    ~AudioDecoder() override;
    
    bool Initialize(const AVCodecParameters* params) override;
    int SendPacket(AVPacket* packet) override;
    int ReceiveFrame(AVFrame* frame) override;
    void Flush() override;
    void Release() override;
    DecoderInfo GetInfo() const override;
    
    // 音频特有
    int GetSampleRate() const { return sample_rate_; }
    int GetChannels() const { return channels_; }
    AVSampleFormat GetSampleFormat() const { return sample_format_; }
    
private:
    AVCodecContext* codec_ctx_;
    const AVCodec* codec_;
    
    // 音频信息
    int sample_rate_;
    int channels_;
    AVSampleFormat sample_format_;
    
    // 解码统计
    int64_t samples_decoded_;
};
```

### 5.4 Renderer模块

#### 功能职责
- 视频渲染（OpenGL ES/Metal/DirectX）
- 音频播放（平台音频API）
- 音视频同步
- 渲染质量控制

#### 类设计

```cpp
// 渲染器基类
class Renderer {
public:
    virtual ~Renderer() = default;
    
    virtual bool Initialize(void* native_window) = 0;
    virtual void Release() = 0;
    virtual void Resize(int width, int height) = 0;
};

// 视频渲染器
class VideoRenderer : public Renderer {
public:
    virtual bool RenderFrame(AVFrame* frame) = 0;
    virtual void SetRenderRect(const Rect& rect) = 0;
    virtual void SetAspectRatio(AspectRatio ratio) = 0;
};

// 音频渲染器
class AudioRenderer {
public:
    virtual ~AudioRenderer() = default;
    
    virtual bool Initialize(int sample_rate, int channels, 
                           AVSampleFormat format) = 0;
    virtual int Write(const uint8_t** data, int samples) = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;
    virtual void Flush() = 0;
    virtual void Release() = 0;
    
    // 获取音频时钟
    virtual int64_t GetPlaybackPosition() = 0;
    virtual int GetBufferLevel() = 0;
};

// 音视频同步器
class AVSynchronizer {
public:
    enum SyncMode {
        SYNC_AUDIO_MASTER,    // 音频为主时钟
        SYNC_VIDEO_MASTER,    // 视频为主时钟
        SYNC_EXTERNAL_CLOCK   // 外部时钟
    };
    
    AVSynchronizer();
    
    // 设置同步模式
    void SetSyncMode(SyncMode mode);
    
    // 更新时钟
    void UpdateAudioClock(int64_t pts, int64_t time);
    void UpdateVideoClock(int64_t pts, int64_t time);
    
    // 获取主时钟
    int64_t GetMasterClock() const;
    
    // 计算视频延迟
    double ComputeVideoDelay(int64_t video_pts) const;
    
    // 计算音频延迟
    double ComputeAudioDelay(int64_t audio_pts) const;
    
private:
    SyncMode sync_mode_;
    
    // 时钟
    Clock audio_clock_;
    Clock video_clock_;
    Clock external_clock_;
    
    // 同步阈值
    double video_delay_threshold_;    // 视频延迟阈值(ms)
    double audio_delay_threshold_;    // 音频延迟阈值(ms)
    
    // 同步统计
    int64_t last_sync_time_;
    double drift_;                    // 时钟漂移
};

// 时钟结构
struct Clock {
    int64_t pts;           // 当前PTS
    int64_t last_update;   // 最后更新时间
    double speed;          // 播放速度
    
    int64_t GetCurrentTime() const {
        int64_t now = GetCurrentTimeMs();
        return pts + (now - last_update) * speed;
    }
};
```

### 5.5 Encoder模块（第三阶段）

#### 功能职责
- 视频编码（H264/H265）
- 音频编码（AAC）
- 格式封装（MP4/FLV）
- 码率控制

#### 类设计

```cpp
// 编码器基类
class Encoder {
public:
    virtual ~Encoder() = default;
    
    virtual bool Initialize(const EncoderConfig& config) = 0;
    virtual int SendFrame(AVFrame* frame) = 0;
    virtual int ReceivePacket(AVPacket* packet) = 0;
    virtual void Flush() = 0;
    virtual void Release() = 0;
};

// 视频编码器配置
struct VideoEncoderConfig {
    int width;
    int height;
    int frame_rate;
    int64_t bitrate;
    int gop_size;
    AVCodecID codec_id;           // AV_CODEC_ID_H264 / AV_CODEC_ID_HEVC
    bool use_hardware;            // 使用硬件编码
    int quality;                  // 质量等级 (0-100)
};

// 音频编码器配置
struct AudioEncoderConfig {
    int sample_rate;
    int channels;
    int64_t bitrate;
    AVCodecID codec_id;           // AV_CODEC_ID_AAC
};

// 视频编码器
class VideoEncoder : public Encoder {
public:
    VideoEncoder();
    ~VideoEncoder() override;
    
    bool Initialize(const EncoderConfig& config) override;
    int SendFrame(AVFrame* frame) override;
    int ReceivePacket(AVPacket* packet) override;
    void Flush() override;
    void Release() override;
    
    // 动态码率调整
    void SetBitrate(int64_t bitrate);
    
    // 强制关键帧
    void ForceKeyframe();
    
private:
    AVCodecContext* codec_ctx_;
    const AVCodec* codec_;
    
    // 硬件编码
    std::unique_ptr<HWEncoderContext> hw_ctx_;
    bool use_hwaccel_;
    
    // 编码统计
    int64_t frames_encoded_;
    int64_t bytes_encoded_;
    double current_bitrate_;
};

// 媒体录制器（封装器）
class MediaRecorder {
public:
    MediaRecorder();
    ~MediaRecorder();
    
    // 开始录制
    bool StartRecording(const std::string& output_path,
                       const RecorderConfig& config);
    
    // 停止录制
    void StopRecording();
    
    // 写入视频帧
    bool WriteVideoFrame(AVFrame* frame);
    
    // 写入音频帧
    bool WriteAudioFrame(AVFrame* frame);
    
    // 获取录制状态
    RecorderStatus GetStatus() const;
    
private:
    AVFormatContext* format_ctx_;
    
    std::unique_ptr<VideoEncoder> video_encoder_;
    std::unique_ptr<AudioEncoder> audio_encoder_;
    
    int video_stream_index_;
    int audio_stream_index_;
    
    int64_t start_time_;
    std::atomic<bool> is_recording_;
    
    bool InitializeOutput(const std::string& path);
};
```

### 5.6 数据旁路模块（第四阶段）

#### 功能职责
- 原始视频帧回调（YUV/RGB）
- 原始音频帧回调（PCM）
- 编码后数据回调（H264/H265/AAC）
- 自定义处理支持

#### 类设计

```cpp
// 数据回调接口
class DataCallback {
public:
    virtual ~DataCallback() = default;
    
    // 原始视频帧回调
    virtual void OnRawVideoFrame(AVFrame* frame) = 0;
    
    // 原始音频帧回调
    virtual void OnRawAudioFrame(AVFrame* frame) = 0;
    
    // 编码后视频数据回调
    virtual void OnEncodedVideoPacket(AVPacket* packet) = 0;
    
    // 编码后音频数据回调
    virtual void OnEncodedAudioPacket(AVPacket* packet) = 0;
};

// 数据旁路管理器
class DataBypassManager {
public:
    DataBypassManager();
    ~DataBypassManager();
    
    // 注册回调
    void RegisterCallback(DataCallback* callback);
    void UnregisterCallback(DataCallback* callback);
    
    // 设置回调类型
    void EnableRawVideoCallback(bool enable);
    void EnableRawAudioCallback(bool enable);
    void EnableEncodedVideoCallback(bool enable);
    void EnableEncodedAudioCallback(bool enable);
    
    // 数据分发
    void DispatchRawVideoFrame(AVFrame* frame);
    void DispatchRawAudioFrame(AVFrame* frame);
    void DispatchEncodedVideoPacket(AVPacket* packet);
    void DispatchEncodedAudioPacket(AVPacket* packet);
    
    // 设置回调线程
    void SetCallbackThread(std::shared_ptr<Thread> thread);
    
private:
    std::vector<DataCallback*> callbacks_;
    std::mutex callback_mutex_;
    
    // 回调开关
    bool enable_raw_video_;
    bool enable_raw_audio_;
    bool enable_encoded_video_;
    bool enable_encoded_audio_;
    
    // 回调线程
    std::shared_ptr<Thread> callback_thread_;
    
    // 帧缓冲池（避免频繁分配）
    FramePool frame_pool_;
};

// 数据处理器（示例：水印添加）
class WatermarkProcessor : public DataCallback {
public:
    WatermarkProcessor();
    
    void SetWatermarkImage(const std::string& image_path);
    void SetWatermarkPosition(int x, int y);
    
    // 处理原始视频帧，添加水印
    void OnRawVideoFrame(AVFrame* frame) override;
    void OnRawAudioFrame(AVFrame* frame) override {}
    void OnEncodedVideoPacket(AVPacket* packet) override {}
    void OnEncodedAudioPacket(AVPacket* packet) override {}
    
private:
    AVFrame* watermark_frame_;
    int pos_x_, pos_y_;
    
    void ApplyWatermark(AVFrame* dst, AVFrame* watermark);
};
```



---

## 6. 性能优化方案

### 6.1 零拷贝优化

#### 零拷贝架构

```
传统方式（有拷贝）:
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│  解码器   │───▶│ 系统内存  │───▶│ 纹理上传  │───▶│  GPU渲染  │
│ (GPU)    │    │ (CPU拷贝) │    │ (CPU-GPU) │    │          │
└──────────┘    └──────────┘    └──────────┘    └──────────┘

零拷贝方式:
┌──────────┐    ┌──────────┐    ┌──────────┐
│  解码器   │───▶│ GPU纹理  │───▶│  GPU渲染  │
│ (GPU)    │    │ (共享)   │    │          │
└──────────┘    └──────────┘    └──────────┘
```

#### 各平台零拷贝实现

**iOS/macOS - VideoToolbox零拷贝**

```cpp
// VideoToolbox零拷贝渲染
class VideoToolboxZeroCopyRenderer {
public:
    bool Initialize(CVPixelBufferRef pixel_buffer) {
        // 直接绑定CVPixelBuffer到Metal纹理
        CVMetalTextureCacheCreate(nullptr, nullptr, 
            metal_device_, nullptr, &texture_cache_);
        
        CVMetalTextureRef metal_texture = nullptr;
        CVMetalTextureCacheCreateTextureFromImage(
            nullptr,                    // allocator
            texture_cache_,             // texture cache
            pixel_buffer,               // source image
            nullptr,                    // texture attributes
            MTLPixelFormatBGRA8Unorm,   // pixel format
            width, height,              // dimensions
            0,                          // plane index
            &metal_texture              // texture out
        );
        
        // 直接使用纹理渲染，无需CPU拷贝
        id<MTLTexture> texture = CVMetalTextureGetTexture(metal_texture);
        
        // 渲染...
        
        CVBufferRelease(metal_texture);
        return true;
    }
    
private:
    CVMetalTextureCacheRef texture_cache_;
};
```

**Android - MediaCodec零拷贝**

```cpp
// MediaCodec零拷贝渲染
class MediaCodecZeroCopyRenderer {
public:
    // 配置Surface渲染（零拷贝）
    bool ConfigureSurface(ANativeWindow* window) {
        // MediaCodec直接渲染到Surface，无需CPU拷贝
        AMediaCodec_configure(codec_, format_, window, nullptr, 0);
        AMediaCodec_start(codec_);
        return true;
    }
    
    // 或者使用ImageReader获取纹理
    bool ConfigureImageReader() {
        // 创建ImageReader
        AImageReader_new(width, height, AIMAGE_FORMAT_YUV_420_888, 
                         max_images, &image_reader_);
        
        // 获取Surface
        AImageReader_getWindow(image_reader_, &image_reader_window_);
        
        // 配置MediaCodec
        AMediaCodec_configure(codec_, format_, image_reader_window_, nullptr, 0);
        
        return true;
    }
    
private:
    AImageReader* image_reader_;
    ANativeWindow* image_reader_window_;
};
```

**Windows - D3D11VA零拷贝**

```cpp
// D3D11VA零拷贝渲染
class D3D11VAZeroCopyRenderer {
public:
    bool RenderFrame(AVFrame* frame) {
        // AVFrame包含D3D11纹理索引
        if (frame->format == AV_PIX_FMT_D3D11) {
            // 获取纹理
            ID3D11Texture2D* texture = (ID3D11Texture2D*)frame->data[0];
            int index = (int)(uintptr_t)frame->data[1];
            
            // 创建Shader Resource View
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format = DXGI_FORMAT_NV12;
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            srv_desc.Texture2DArray.ArraySize = 1;
            srv_desc.Texture2DArray.FirstArraySlice = index;
            
            ID3D11ShaderResourceView* srv = nullptr;
            device_->CreateShaderResourceView(texture, &srv_desc, &srv);
            
            // 直接渲染，无需拷贝
            context_->PSSetShaderResources(0, 1, &srv);
            
            // 渲染...
            
            srv->Release();
            return true;
        }
        return false;
    }
};
```

### 6.2 缓冲策略

#### 缓冲架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        缓冲策略设计                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  网络缓冲 (Network Buffer)                                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  预加载: 5-10秒                                          │   │
│  │  最大缓冲: 30-60秒                                       │   │
│  │  重新缓冲阈值: 2-5秒                                     │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                   │
│                              ▼                                   │
│  解码缓冲 (Decode Buffer)                                        │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  视频帧队列: 10-30帧                                     │   │
│  │  音频帧队列: 50-100帧                                    │   │
│  │  丢帧阈值: 队列满时丢弃非关键帧                           │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                   │
│                              ▼                                   │
│  渲染缓冲 (Render Buffer)                                        │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  视频渲染队列: 3-5帧                                     │   │
│  │  音频播放缓冲: 100-200ms                                 │   │
│  │  同步窗口: ±40ms                                         │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 缓冲管理实现

```cpp
// 环形缓冲区模板
template<typename T>
class RingBuffer {
public:
    RingBuffer(size_t capacity) 
        : capacity_(capacity), size_(0), head_(0), tail_(0) {
        buffer_.resize(capacity);
    }
    
    bool Push(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (size_ >= capacity_) {
            return false;  // 缓冲区满
        }
        buffer_[tail_] = item;
        tail_ = (tail_ + 1) % capacity_;
        size_++;
        not_empty_.notify_one();
        return true;
    }
    
    bool Pop(T& item, int timeout_ms = -1) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeout_ms > 0) {
            if (!not_empty_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                     [this] { return size_ > 0; })) {
                return false;  // 超时
            }
        } else {
            not_empty_.wait(lock, [this] { return size_ > 0; });
        }
        
        item = buffer_[head_];
        head_ = (head_ + 1) % capacity_;
        size_--;
        not_full_.notify_one();
        return true;
    }
    
    size_t Size() const { return size_; }
    size_t Capacity() const { return capacity_; }
    bool IsEmpty() const { return size_ == 0; }
    bool IsFull() const { return size_ >= capacity_; }
    
    void Clear() {
        std::unique_lock<std::mutex> lock(mutex_);
        head_ = tail_ = size_ = 0;
        not_full_.notify_all();
    }
    
private:
    std::vector<T> buffer_;
    size_t capacity_;
    size_t size_;
    size_t head_, tail_;
    
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

// 帧缓冲区
class FrameBuffer {
public:
    FrameBuffer(size_t video_capacity, size_t audio_capacity);
    
    // 写入帧
    bool PushVideoFrame(AVFrame* frame);
    bool PushAudioFrame(AVFrame* frame);
    
    // 读取帧
    AVFrame* PopVideoFrame(int timeout_ms);
    AVFrame* PopAudioFrame(int timeout_ms);
    
    // 获取缓冲状态
    BufferStatus GetStatus() const;
    
    // 清空缓冲
    void Clear();
    
private:
    RingBuffer<AVFrame*> video_buffer_;
    RingBuffer<AVFrame*> audio_buffer_;
    
    // 帧池
    FramePool frame_pool_;
};
```

### 6.3 线程优化

#### 线程架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        线程架构设计                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐                                                │
│  │ 主线程        │  UI更新、事件处理、API调用                      │
│  │ (Main Thread)│                                                │
│  └──────────────┘                                                │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │ 读取线程      │  │ 视频解码线程  │  │ 音频解码线程  │           │
│  │ (Read Thread)│  │(Video Decode)│  │(Audio Decode)│           │
│  │              │  │              │  │              │           │
│  │ - 网络读取    │  │ - 硬/软解码   │  │ - 软解码      │           │
│  │ - 解封装      │  │ - 帧输出      │  │ - 重采样      │           │
│  └──────────────┘  └──────────────┘  └──────────────┘           │
│         │                  │                  │                  │
│         ▼                  ▼                  ▼                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    帧缓冲队列                            │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                   │
│                              ▼                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │ 视频渲染线程  │  │ 音频播放线程  │  │ 编码线程      │           │
│  │(Video Render)│  │(Audio Play)  │  │(Encode)      │           │
│  │              │  │              │  │              │           │
│  │ - OpenGL/Metal│  │ - AudioTrack │  │ - 视频编码    │           │
│  │ - 同步渲染    │  │ - OpenSL ES  │  │ - 音频编码    │           │
│  │              │  │ - AAudio     │  │ - 封装写入    │           │
│  └──────────────┘  └──────────────┘  └──────────────┘           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 线程池实现

```cpp
// 线程池
class ThreadPool {
public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();
    
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

// 任务优先级队列
class PriorityTaskQueue {
public:
    enum Priority {
        PRIORITY_HIGH = 0,    // 解码、渲染
        PRIORITY_NORMAL = 1,  // 网络读取
        PRIORITY_LOW = 2      // 日志、统计
    };
    
    void Push(const std::function<void()>& task, Priority priority);
    std::function<void()> Pop();
    bool IsEmpty() const;
    
private:
    std::priority_queue<
        std::pair<Priority, std::function<void()>>,
        std::vector<std::pair<Priority, std::function<void()>>>,
        std::greater<std::pair<Priority, std::function<void()>>>
    > queue_;
    
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};
```

### 6.4 内存优化

#### 内存池实现

```cpp
// 内存池
class MemoryPool {
public:
    MemoryPool(size_t block_size, size_t pool_size);
    ~MemoryPool();
    
    void* Allocate();
    void Free(void* ptr);
    
private:
    size_t block_size_;
    size_t pool_size_;
    
    char* pool_;
    std::queue<void*> free_blocks_;
    std::mutex mutex_;
};

// AVFrame池
class FramePool {
public:
    FramePool(size_t pool_size);
    ~FramePool();
    
    AVFrame* Acquire();
    void Release(AVFrame* frame);
    
private:
    std::queue<AVFrame*> available_frames_;
    std::mutex mutex_;
};

// AVPacket池
class PacketPool {
public:
    PacketPool(size_t pool_size);
    ~PacketPool();
    
    AVPacket* Acquire();
    void Release(AVPacket* packet);
    
private:
    std::queue<AVPacket*> available_packets_;
    std::mutex mutex_;
};
```

#### 内存优化策略

```cpp
// 内存优化管理器
class MemoryOptimizer {
public:
    // 预分配帧缓冲区
    void PreallocateFrames(int width, int height, AVPixelFormat format, int count) {
        for (int i = 0; i < count; i++) {
            AVFrame* frame = av_frame_alloc();
            frame->width = width;
            frame->height = height;
            frame->format = format;
            av_frame_get_buffer(frame, 32);  // 32字节对齐
            frame_pool_.Release(frame);
        }
    }
    
    // 预分配Packet缓冲区
    void PreallocatePackets(int size) {
        for (int i = 0; i < size; i++) {
            AVPacket* packet = av_packet_alloc();
            packet_pool_.Release(packet);
        }
    }
    
    // 内存使用监控
    struct MemoryStats {
        size_t total_allocated;
        size_t total_used;
        size_t frame_pool_size;
        size_t packet_pool_size;
    };
    
    MemoryStats GetStats() const;
    
private:
    FramePool frame_pool_;
    PacketPool packet_pool_;
};
```

---

## 7. 各平台差异处理

### 7.1 平台抽象层设计

```
┌─────────────────────────────────────────────────────────────────┐
│                    平台抽象层 (PAL)                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    PlatformInterface                      │   │
│  │  - CreateWindow()                                        │   │
│  │  - CreateRenderer()                                      │   │
│  │  - CreateAudioPlayer()                                   │   │
│  │  - CreateHWDecoder()                                     │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                   │
│          ┌───────────────────┼───────────────────┐              │
│          ▼                   ▼                   ▼              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   iOS PAL    │  │  Android PAL │  │  Windows PAL │          │
│  │              │  │              │  │              │          │
│  │ - Metal      │  │ - GLES       │  │ - D3D11      │          │
│  │ - VideoToolbox│  │ - MediaCodec │  │ - D3D11VA    │          │
│  │ - AudioUnit  │  │ - OpenSL ES  │  │ - WASAPI     │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 平台特定代码组织

```
project/
├── src/
│   ├── core/                    # 平台无关核心代码
│   │   ├── demuxer.cpp
│   │   ├── decoder.cpp
│   │   ├── renderer.cpp
│   │   └── ...
│   │
│   ├── platform/                # 平台抽象层
│   │   ├── platform_interface.h
│   │   ├── platform_factory.h
│   │   └── platform_factory.cpp
│   │
│   ├── platform_ios/            # iOS平台实现
│   │   ├── ios_window.h
│   │   ├── ios_window.mm
│   │   ├── metal_renderer.h
│   │   ├── metal_renderer.mm
│   │   ├── videotoolbox_decoder.h
│   │   ├── videotoolbox_decoder.mm
│   │   └── ios_audio_player.mm
│   │
│   ├── platform_android/        # Android平台实现
│   │   ├── android_window.h
│   │   ├── android_window.cpp
│   │   ├── gles_renderer.h
│   │   ├── gles_renderer.cpp
│   │   ├── mediacodec_decoder.h
│   │   ├── mediacodec_decoder.cpp
│   │   └── android_audio_player.cpp
│   │
│   ├── platform_macos/          # macOS平台实现
│   │   ├── macos_window.h
│   │   ├── macos_window.mm
│   │   ├── metal_renderer.h
│   │   ├── metal_renderer.mm
│   │   └── macos_audio_player.mm
│   │
│   └── platform_windows/        # Windows平台实现
│       ├── windows_window.h
│       ├── windows_window.cpp
│       ├── d3d11_renderer.h
│       ├── d3d11_renderer.cpp
│       ├── d3d11va_decoder.h
│       ├── d3d11va_decoder.cpp
│       └── windows_audio_player.cpp
│
└── include/
    └── sdk/
        └── public_api.h         # 公共API头文件
```

### 7.3 条件编译策略

```cpp
// platform_config.h
#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

// 平台检测
#if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #define PLATFORM_IOS 1
        #define PLATFORM_MOBILE 1
    #else
        #define PLATFORM_MACOS 1
        #define PLATFORM_DESKTOP 1
    #endif
#elif defined(__ANDROID__)
    #define PLATFORM_ANDROID 1
    #define PLATFORM_MOBILE 1
#elif defined(_WIN32)
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_DESKTOP 1
#endif

// 功能开关
#if PLATFORM_IOS || PLATFORM_MACOS
    #define SUPPORT_METAL 1
    #define SUPPORT_VIDEOTOOLBOX 1
    #define SUPPORT_AUDIOTOOLBOX 1
#endif

#if PLATFORM_ANDROID
    #define SUPPORT_GLES 1
    #define SUPPORT_MEDIACODEC 1
    #define SUPPORT_OPENSLES 1
#endif

#if PLATFORM_WINDOWS
    #define SUPPORT_D3D11 1
    #define SUPPORT_D3D11VA 1
    #define SUPPORT_WASAPI 1
#endif

#endif // PLATFORM_CONFIG_H
```

### 7.4 运行时检测

```cpp
// 硬件能力检测
class HardwareCapability {
public:
    // 检测硬解码支持
    static bool IsHardwareDecodeSupported(AVCodecID codec_id) {
        const AVCodec* codec = avcodec_find_decoder(codec_id);
        if (!codec) return false;
        
        for (const AVCodecHWConfig* config = avcodec_get_hw_config(codec, 0);
             config; config = avcodec_get_hw_config(codec, config->index + 1)) {
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
                // 检测设备是否可用
                AVBufferRef* device_ctx = nullptr;
                int ret = av_hwdevice_ctx_create(&device_ctx, 
                    config->device_type, nullptr, nullptr, 0);
                if (ret >= 0) {
                    av_buffer_unref(&device_ctx);
                    return true;
                }
            }
        }
        return false;
    }
    
    // 检测GPU支持
    static GPUInfo GetGPUInfo() {
        GPUInfo info;
        
#if PLATFORM_IOS || PLATFORM_MACOS
        // Metal设备检测
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        info.name = [device.name UTF8String];
        info.supports_metal = true;
#endif

#if PLATFORM_ANDROID
        // GLES版本检测
        const char* version = (const char*)glGetString(GL_VERSION);
        info.gles_version = version;
        info.supports_gles3 = (strstr(version, "OpenGL ES 3.") != nullptr);
#endif

#if PLATFORM_WINDOWS
        // DirectX特性等级检测
        D3D_FEATURE_LEVEL feature_level;
        ID3D11Device* device = nullptr;
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                          nullptr, 0, D3D11_SDK_VERSION, &device, 
                          &feature_level, nullptr);
        info.d3d_feature_level = feature_level;
        device->Release();
#endif
        
        return info;
    }
    
    // 检测系统内存
    static size_t GetSystemMemory() {
#if PLATFORM_IOS || PLATFORM_MACOS
        vm_statistics64_data_t vm_stats;
        mach_msg_type_number_t info_count = HOST_VM_INFO64_COUNT;
        kern_return_t kern = host_statistics64(mach_host_self(),
                                               HOST_VM_INFO64,
                                               (host_info64_t)&vm_stats,
                                               &info_count);
        if (kern == KERN_SUCCESS) {
            return vm_stats.free_count * vm_page_size;
        }
#elif PLATFORM_ANDROID
        struct sysinfo info;
        sysinfo(&info);
        return info.freeram;
#elif PLATFORM_WINDOWS
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);
        return status.ullAvailPhys;
#endif
        return 0;
    }
};
```

### 7.5 平台特定实现示例

```cpp
// PlatformFactory.cpp
std::unique_ptr<VideoRenderer> PlatformFactory::CreateVideoRenderer() {
#if PLATFORM_IOS || PLATFORM_MACOS
    if (HardwareCapability::IsMetalSupported()) {
        return std::make_unique<MetalRenderer>();
    }
#elif PLATFORM_ANDROID
    return std::make_unique<GLESRenderer>();
#elif PLATFORM_WINDOWS
    return std::make_unique<D3D11Renderer>();
#endif
    return nullptr;
}

std::unique_ptr<VideoDecoder> PlatformFactory::CreateHardwareDecoder(
    AVCodecContext* codec_ctx) {
#if PLATFORM_IOS || PLATFORM_MACOS
    return std::make_unique<VideoToolboxDecoder>();
#elif PLATFORM_ANDROID
    return std::make_unique<MediaCodecDecoder>();
#elif PLATFORM_WINDOWS
    return std::make_unique<D3D11VADecoder>();
#endif
    return nullptr;
}

std::unique_ptr<AudioRenderer> PlatformFactory::CreateAudioRenderer() {
#if PLATFORM_IOS || PLATFORM_MACOS
    return std::make_unique<AudioUnitRenderer>();
#elif PLATFORM_ANDROID
    // Android 8.0+ 使用AAudio，否则使用OpenSL ES
    if (android_get_device_api_level() >= 26) {
        return std::make_unique<AAudioRenderer>();
    } else {
        return std::make_unique<OpenSLRenderer>();
    }
#elif PLATFORM_WINDOWS
    return std::make_unique<WASAPIRenderer>();
#endif
    return nullptr;
}
```



---

## 8. 关键代码片段

### 8.1 FFmpeg初始化和解封装

```cpp
// FFmpeg播放器初始化
class FFmpegPlayer {
public:
    FFmpegPlayer() : format_ctx_(nullptr), video_codec_ctx_(nullptr), 
                     audio_codec_ctx_(nullptr), video_stream_index_(-1), 
                     audio_stream_index_(-1) {}
    
    ~FFmpegPlayer() {
        Cleanup();
    }
    
    bool Initialize(const std::string& url) {
        // 1. 分配格式上下文
        format_ctx_ = avformat_alloc_context();
        if (!format_ctx_) {
            LOG_ERROR("Failed to allocate format context");
            return false;
        }
        
        // 2. 设置网络超时和打断回调
        format_ctx_->interrupt_callback.callback = InterruptCallback;
        format_ctx_->interrupt_callback.opaque = this;
        
        // 3. 打开输入
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "timeout", "5000000", 0);  // 5秒超时
        av_dict_set(&opts, "buffer_size", "65536", 0);
        
        int ret = avformat_open_input(&format_ctx_, url.c_str(), nullptr, &opts);
        av_dict_free(&opts);
        
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            LOG_ERROR("Failed to open input: %s", errbuf);
            return false;
        }
        
        // 4. 获取流信息
        ret = avformat_find_stream_info(format_ctx_, nullptr);
        if (ret < 0) {
            LOG_ERROR("Failed to find stream info");
            return false;
        }
        
        // 5. 查找视频和音频流
        for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
            AVStream* stream = format_ctx_->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && 
                video_stream_index_ < 0) {
                video_stream_index_ = i;
            } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && 
                       audio_stream_index_ < 0) {
                audio_stream_index_ = i;
            }
        }
        
        // 6. 初始化视频解码器
        if (video_stream_index_ >= 0) {
            if (!InitializeVideoDecoder()) {
                LOG_ERROR("Failed to initialize video decoder");
                return false;
            }
        }
        
        // 7. 初始化音频解码器
        if (audio_stream_index_ >= 0) {
            if (!InitializeAudioDecoder()) {
                LOG_ERROR("Failed to initialize audio decoder");
                return false;
            }
        }
        
        LOG_INFO("FFmpegPlayer initialized successfully");
        LOG_INFO("  Video: %s", video_stream_index_ >= 0 ? "yes" : "no");
        LOG_INFO("  Audio: %s", audio_stream_index_ >= 0 ? "yes" : "no");
        
        return true;
    }
    
    void Cleanup() {
        if (video_codec_ctx_) {
            avcodec_free_context(&video_codec_ctx_);
        }
        if (audio_codec_ctx_) {
            avcodec_free_context(&audio_codec_ctx_);
        }
        if (format_ctx_) {
            avformat_close_input(&format_ctx_);
        }
    }
    
private:
    bool InitializeVideoDecoder() {
        AVStream* stream = format_ctx_->streams[video_stream_index_];
        
        // 查找解码器
        const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codec) {
            LOG_ERROR("Codec not found");
            return false;
        }
        
        // 分配解码器上下文
        video_codec_ctx_ = avcodec_alloc_context3(codec);
        if (!video_codec_ctx_) {
            LOG_ERROR("Failed to allocate codec context");
            return false;
        }
        
        // 复制编解码器参数
        int ret = avcodec_parameters_to_context(video_codec_ctx_, stream->codecpar);
        if (ret < 0) {
            LOG_ERROR("Failed to copy codec parameters");
            return false;
        }
        
        // 设置硬件加速
        video_codec_ctx_->get_format = GetHWFormat;
        
        // 打开解码器
        ret = avcodec_open2(video_codec_ctx_, codec, nullptr);
        if (ret < 0) {
            LOG_ERROR("Failed to open codec");
            return false;
        }
        
        LOG_INFO("Video decoder: %s, %dx%d", 
                 codec->name, 
                 video_codec_ctx_->width, 
                 video_codec_ctx_->height);
        
        return true;
    }
    
    bool InitializeAudioDecoder() {
        AVStream* stream = format_ctx_->streams[audio_stream_index_];
        
        const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codec) {
            LOG_ERROR("Audio codec not found");
            return false;
        }
        
        audio_codec_ctx_ = avcodec_alloc_context3(codec);
        if (!audio_codec_ctx_) {
            LOG_ERROR("Failed to allocate audio codec context");
            return false;
        }
        
        int ret = avcodec_parameters_to_context(audio_codec_ctx_, stream->codecpar);
        if (ret < 0) {
            LOG_ERROR("Failed to copy audio codec parameters");
            return false;
        }
        
        ret = avcodec_open2(audio_codec_ctx_, codec, nullptr);
        if (ret < 0) {
            LOG_ERROR("Failed to open audio codec");
            return false;
        }
        
        LOG_INFO("Audio decoder: %s, %dHz, %d channels", 
                 codec->name,
                 audio_codec_ctx_->sample_rate,
                 audio_codec_ctx_->ch_layout.nb_channels);
        
        return true;
    }
    
    static int InterruptCallback(void* ctx) {
        FFmpegPlayer* player = static_cast<FFmpegPlayer*>(ctx);
        return player->interrupt_requested_.load() ? 1 : 0;
    }
    
    static enum AVPixelFormat GetHWFormat(AVCodecContext* ctx, 
                                          const enum AVPixelFormat* pix_fmts) {
        const enum AVPixelFormat* p;
        for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
            // 优先选择硬件格式
            switch (*p) {
                case AV_PIX_FMT_VIDEOTOOLBOX:
                case AV_PIX_FMT_MEDIACODEC:
                case AV_PIX_FMT_D3D11:
                case AV_PIX_FMT_DXVA2_VLD:
                    LOG_INFO("Using hardware format: %s", av_get_pix_fmt_name(*p));
                    return *p;
                default:
                    break;
            }
        }
        // 回退到第一个可用格式
        return pix_fmts[0];
    }
    
    AVFormatContext* format_ctx_;
    AVCodecContext* video_codec_ctx_;
    AVCodecContext* audio_codec_ctx_;
    int video_stream_index_;
    int audio_stream_index_;
    std::atomic<bool> interrupt_requested_{false};
};
```

### 8.2 硬解码初始化（VideoToolbox）

```cpp
// VideoToolbox硬解码器完整实现
class VideoToolboxDecoder {
public:
    VideoToolboxDecoder() 
        : session_(nullptr), format_desc_(nullptr), 
          hw_device_ctx_(nullptr), hw_frames_ctx_(nullptr) {}
    
    ~VideoToolboxDecoder() {
        Release();
    }
    
    bool Initialize(AVCodecContext* codec_ctx) {
        // 1. 创建格式描述
        if (!CreateFormatDescription(codec_ctx)) {
            LOG_ERROR("Failed to create format description");
            return false;
        }
        
        // 2. 配置解码器规格
        CFMutableDictionaryRef decoder_spec = CFDictionaryCreateMutable(
            nullptr, 0, &kCFTypeDictionaryKeyCallBacks, 
            &kCFTypeDictionaryValueCallBacks);
        
        // 要求硬件加速
        CFDictionarySetValue(decoder_spec,
            kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
            kCFBooleanTrue);
        
        // 3. 配置输出图像缓冲区属性
        CFMutableDictionaryRef output_attrs = CFDictionaryCreateMutable(
            nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        
        // 设置像素格式
        int32_t pixel_format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
        CFNumberRef pixel_format_num = CFNumberCreate(nullptr, kCFNumberSInt32Type, 
                                                       &pixel_format);
        CFDictionarySetValue(output_attrs, kCVPixelBufferPixelFormatTypeKey, 
                            pixel_format_num);
        CFRelease(pixel_format_num);
        
        // 设置尺寸
        int32_t width = codec_ctx->width;
        int32_t height = codec_ctx->height;
        CFNumberRef width_num = CFNumberCreate(nullptr, kCFNumberSInt32Type, &width);
        CFNumberRef height_num = CFNumberCreate(nullptr, kCFNumberSInt32Type, &height);
        CFDictionarySetValue(output_attrs, kCVPixelBufferWidthKey, width_num);
        CFDictionarySetValue(output_attrs, kCVPixelBufferHeightKey, height_num);
        CFRelease(width_num);
        CFRelease(height_num);
        
        // 4. 创建输出回调
        VTDecompressionOutputCallbackRecord callback_record;
        callback_record.decompressionOutputCallback = OutputCallback;
        callback_record.decompressionOutputRefCon = this;
        
        // 5. 创建解码会话
        OSStatus status = VTDecompressionSessionCreate(
            nullptr,                    // allocator
            format_desc_,               // video format description
            decoder_spec,               // video decoder specification
            output_attrs,               // destination image buffer attributes
            &callback_record,           // output callback
            &session_                   // decompression session out
        );
        
        CFRelease(decoder_spec);
        CFRelease(output_attrs);
        
        if (status != noErr) {
            LOG_ERROR("Failed to create VTDecompressionSession: %d", (int)status);
            return false;
        }
        
        LOG_INFO("VideoToolbox decoder initialized: %dx%d", width, height);
        return true;
    }
    
    int DecodePacket(AVPacket* packet, AVFrame* frame) {
        if (!session_) return -1;
        
        // 创建CMBlockBuffer
        CMBlockBufferRef block_buffer = nullptr;
        OSStatus status = CMBlockBufferCreateWithMemoryBlock(
            nullptr,                    // allocator
            packet->data,               // memory block
            packet->size,               // block length
            kCFAllocatorNull,           // block allocator
            nullptr,                    // custom block source
            0,                          // offset
            packet->size,               // data length
            0,                          // flags
            &block_buffer               // block buffer out
        );
        
        if (status != noErr) {
            LOG_ERROR("Failed to create block buffer");
            return -1;
        }
        
        // 创建CMSampleBuffer
        CMSampleBufferRef sample_buffer = nullptr;
        status = CMSampleBufferCreate(
            nullptr,                    // allocator
            block_buffer,               // data buffer
            true,                       // data ready
            nullptr,                    // make data ready callback
            nullptr,                    // make data ready ref con
            format_desc_,               // format description
            1,                          // num samples
            0,                          // sample timing entry count
            nullptr,                    // sample timing array
            0,                          // sample size entry count
            nullptr,                    // sample size array
            &sample_buffer              // sample buffer out
        );
        
        CFRelease(block_buffer);
        
        if (status != noErr) {
            LOG_ERROR("Failed to create sample buffer");
            return -1;
        }
        
        // 设置解码标志
        VTDecodeFrameFlags decode_flags = kVTDecodeFrame_EnableAsynchronousDecompression;
        VTDecodeInfoFlags info_flags_out;
        
        // 发送解码请求
        status = VTDecompressionSessionDecodeFrame(
            session_,
            sample_buffer,
            decode_flags,
            nullptr,                    // source frame ref con
            &info_flags_out             // info flags out
        );
        
        CFRelease(sample_buffer);
        
        if (status != noErr) {
            LOG_ERROR("Failed to decode frame: %d", (int)status);
            return -1;
        }
        
        // 获取解码后的帧（异步回调中处理）
        // 这里简化处理，实际应该等待回调
        
        return 0;
    }
    
    void Flush() {
        if (session_) {
            VTDecompressionSessionFinishDelayedFrames(session_);
        }
    }
    
    void Release() {
        if (session_) {
            VTDecompressionSessionInvalidate(session_);
            CFRelease(session_);
            session_ = nullptr;
        }
        if (format_desc_) {
            CFRelease(format_desc_);
            format_desc_ = nullptr;
        }
    }
    
private:
    bool CreateFormatDescription(AVCodecContext* codec_ctx) {
        // 根据编解码器类型创建格式描述
        if (codec_ctx->codec_id == AV_CODEC_ID_H264) {
            return CreateH264FormatDescription(codec_ctx);
        } else if (codec_ctx->codec_id == AV_CODEC_ID_HEVC) {
            return CreateHEVCFormatDescription(codec_ctx);
        }
        return false;
    }
    
    bool CreateH264FormatDescription(AVCodecContext* codec_ctx) {
        // 需要SPS和PPS来创建格式描述
        if (!codec_ctx->extradata || codec_ctx->extradata_size < 7) {
            LOG_ERROR("No extradata for H264");
            return false;
        }
        
        // 解析SPS和PPS
        const uint8_t* extradata = codec_ctx->extradata;
        size_t extradata_size = codec_ctx->extradata_size;
        
        // 创建CMVideoFormatDescription
        OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
            nullptr,                    // allocator
            2,                          // parameter set count (SPS + PPS)
            &extradata,                 // parameter set pointers
            &extradata_size,            // parameter set sizes
            4,                          // NAL unit header length
            &format_desc_               // format description out
        );
        
        return status == noErr;
    }
    
    bool CreateHEVCFormatDescription(AVCodecContext* codec_ctx) {
        // H265/HEVC格式描述创建
        if (!codec_ctx->extradata || codec_ctx->extradata_size < 23) {
            LOG_ERROR("No extradata for HEVC");
            return false;
        }
        
        // 解析VPS、SPS、PPS
        // ...
        
        OSStatus status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(
            nullptr,
            3,                          // VPS + SPS + PPS
            nullptr,                    // parameter set pointers
            nullptr,                    // parameter set sizes
            4,                          // NAL unit header length
            nullptr,                    // extensions
            &format_desc_
        );
        
        return status == noErr;
    }
    
    static void OutputCallback(void* decompression_output_ref_con,
                               void* source_frame_ref_con,
                               OSStatus status,
                               VTDecodeInfoFlags info_flags,
                               CVImageBufferRef image_buffer,
                               CMTime presentation_time_stamp,
                               CMTime presentation_duration) {
        VideoToolboxDecoder* decoder = static_cast<VideoToolboxDecoder*>(
            decompression_output_ref_con);
        
        if (status != noErr) {
            LOG_ERROR("Decode error: %d", (int)status);
            return;
        }
        
        if (!image_buffer) {
            LOG_WARNING("No image buffer");
            return;
        }
        
        // 处理解码后的图像
        // 可以将CVPixelBuffer转换为AVFrame或直接渲染
        
        decoder->HandleDecodedFrame(image_buffer, presentation_time_stamp);
    }
    
    void HandleDecodedFrame(CVImageBufferRef image_buffer, CMTime pts) {
        // 将CVPixelBuffer包装为AVFrame
        // 或通知渲染器直接渲染
    }
    
    VTDecompressionSessionRef session_;
    CMVideoFormatDescriptionRef format_desc_;
    AVBufferRef* hw_device_ctx_;
    AVBufferRef* hw_frames_ctx_;
};
```

### 8.3 音视频同步算法

```cpp
// 音视频同步器完整实现
class AVSynchronizer {
public:
    enum SyncMode {
        SYNC_AUDIO_MASTER,    // 音频为主时钟（推荐）
        SYNC_VIDEO_MASTER,    // 视频为主时钟
        SYNC_EXTERNAL_CLOCK   // 外部时钟
    };
    
    AVSynchronizer() 
        : sync_mode_(SYNC_AUDIO_MASTER),
          max_frame_duration_(10.0),
          frame_drops_early_(0),
          frame_drops_late_(0) {
        InitClock(&audio_clock_);
        InitClock(&video_clock_);
        InitClock(&external_clock_);
    }
    
    // 设置同步模式
    void SetSyncMode(SyncMode mode) {
        sync_mode_ = mode;
    }
    
    // 初始化时钟
    void InitClock(Clock* clock) {
        clock->pts = AV_NOPTS_VALUE;
        clock->pts_drift = 0;
        clock->last_updated = 0;
        clock->speed = 1.0;
        clock->paused = 0;
    }
    
    // 设置时钟
    void SetClock(Clock* clock, double pts, int64_t time) {
        clock->pts = pts;
        clock->last_updated = time;
        clock->pts_drift = clock->pts - time / 1000000.0;
    }
    
    // 获取时钟
    double GetClock(Clock* clock) {
        if (clock->paused) {
            return clock->pts;
        } else {
            double time = GetCurrentTimeSec();
            return clock->pts_drift + time;
        }
    }
    
    // 更新音频时钟
    void UpdateAudioClock(double pts, int64_t time) {
        SetClock(&audio_clock_, pts, time);
    }
    
    // 更新视频时钟
    void UpdateVideoClock(double pts, int64_t time) {
        SetClock(&video_clock_, pts, time);
    }
    
    // 获取主时钟
    double GetMasterClock() {
        switch (sync_mode_) {
            case SYNC_AUDIO_MASTER:
                return GetClock(&audio_clock_);
            case SYNC_VIDEO_MASTER:
                return GetClock(&video_clock_);
            case SYNC_EXTERNAL_CLOCK:
                return GetClock(&external_clock_);
            default:
                return GetClock(&audio_clock_);
        }
    }
    
    // 计算视频帧延迟
    double ComputeVideoDelay(double pts, double duration) {
        double delay = duration;
        
        // 如果视频为主时钟，直接返回
        if (sync_mode_ == SYNC_VIDEO_MASTER) {
            return delay;
        }
        
        // 计算与主时钟的差异
        double diff = pts - GetMasterClock();
        
        // 同步阈值
        double sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, 
                                       FFMIN(delay, AV_SYNC_THRESHOLD_MAX));
        
        if (!isnan(diff) && fabs(diff) < max_frame_duration_) {
            if (diff <= -sync_threshold) {
                // 视频落后，减少延迟（加速）
                delay = FFMAX(0, delay + diff);
            } else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) {
                // 视频超前，增加延迟（减速）
                delay = delay + diff;
            } else if (diff >= sync_threshold) {
                // 视频超前，但差异不大，复制帧持续时间
                delay = 2 * delay;
            }
        }
        
        return delay;
    }
    
    // 判断是否应该显示视频帧
    bool ShouldDisplayVideoFrame(double pts, double remaining_time) {
        double time = GetCurrentTimeSec();
        
        // 如果视频为主时钟，总是显示
        if (sync_mode_ == SYNC_VIDEO_MASTER) {
            return true;
        }
        
        // 计算与主时钟的差异
        double diff = pts - GetMasterClock();
        
        // 如果视频超前太多，丢弃
        if (diff > AV_SYNC_THRESHOLD_MAX) {
            frame_drops_early_++;
            LOG_DEBUG("Drop early frame, diff=%.3f", diff);
            return false;
        }
        
        // 如果视频落后太多，丢弃
        if (diff < -AV_SYNC_THRESHOLD_MAX) {
            frame_drops_late_++;
            LOG_DEBUG("Drop late frame, diff=%.3f", diff);
            return false;
        }
        
        // 如果剩余时间大于阈值，等待
        if (remaining_time > 0.001) {
            return false;
        }
        
        return true;
    }
    
    // 获取同步统计
    struct SyncStats {
        int64_t frame_drops_early;
        int64_t frame_drops_late;
        double audio_clock;
        double video_clock;
        double master_clock;
        double diff;
    };
    
    SyncStats GetStats() {
        SyncStats stats;
        stats.frame_drops_early = frame_drops_early_;
        stats.frame_drops_late = frame_drops_late_;
        stats.audio_clock = GetClock(&audio_clock_);
        stats.video_clock = GetClock(&video_clock_);
        stats.master_clock = GetMasterClock();
        stats.diff = stats.video_clock - stats.master_clock;
        return stats;
    }
    
private:
    static constexpr double AV_SYNC_THRESHOLD_MIN = 0.04;   // 40ms
    static constexpr double AV_SYNC_THRESHOLD_MAX = 0.1;    // 100ms
    static constexpr double AV_SYNC_FRAMEDUP_THRESHOLD = 0.1;
    
    struct Clock {
        double pts;           // 时钟基准
        double pts_drift;     // 时钟漂移
        int64_t last_updated; // 最后更新时间
        double speed;         // 播放速度
        int paused;           // 暂停状态
    };
    
    SyncMode sync_mode_;
    Clock audio_clock_;
    Clock video_clock_;
    Clock external_clock_;
    
    double max_frame_duration_;
    int64_t frame_drops_early_;
    int64_t frame_drops_late_;
    
    double GetCurrentTimeSec() {
        return av_gettime() / 1000000.0;
    }
};

// 使用示例
void VideoRenderThread(MediaPlayer* player) {
    AVSynchronizer* sync = player->GetSynchronizer();
    
    while (player->IsPlaying()) {
        AVFrame* frame = player->GetVideoFrameQueue()->Pop();
        if (!frame) continue;
        
        double pts = frame->pts * av_q2d(player->GetVideoTimeBase());
        double duration = player->GetFrameDuration();
        
        // 计算延迟
        double delay = sync->ComputeVideoDelay(pts, duration);
        
        // 计算剩余时间
        double remaining_time = delay - (sync->GetCurrentTimeSec() - last_frame_time);
        
        // 判断是否显示
        if (sync->ShouldDisplayVideoFrame(pts, remaining_time)) {
            // 渲染帧
            player->GetVideoRenderer()->RenderFrame(frame);
            
            // 更新视频时钟
            sync->UpdateVideoClock(pts, av_gettime());
            
            last_frame_time = sync->GetCurrentTimeSec();
        }
        
        av_frame_unref(frame);
    }
}
```

### 8.4 Metal渲染初始化

```cpp
// Metal渲染器完整实现
class MetalRenderer {
public:
    MetalRenderer() 
        : device_(nil), command_queue_(nil), pipeline_state_(nil),
          metal_layer_(nil), texture_cache_(nil) {}
    
    ~MetalRenderer() {
        Release();
    }
    
    bool Initialize(void* native_view) {
        // 1. 获取Metal设备
        device_ = MTLCreateSystemDefaultDevice();
        if (!device_) {
            LOG_ERROR("Failed to create Metal device");
            return false;
        }
        
        // 2. 创建命令队列
        command_queue_ = [device_ newCommandQueue];
        if (!command_queue_) {
            LOG_ERROR("Failed to create command queue");
            return false;
        }
        
        // 3. 获取或创建Metal层
        metal_layer_ = GetMetalLayer(native_view);
        if (!metal_layer_) {
            LOG_ERROR("Failed to get Metal layer");
            return false;
        }
        
        metal_layer_.device = device_;
        metal_layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metal_layer_.framebufferOnly = NO;
        
        // 4. 创建纹理缓存（用于CVPixelBuffer）
        CVMetalTextureCacheCreate(nullptr, nullptr, device_, nullptr, &texture_cache_);
        
        // 5. 创建渲染管线
        if (!CreatePipeline()) {
            LOG_ERROR("Failed to create pipeline");
            return false;
        }
        
        // 6. 创建顶点缓冲区
        if (!CreateVertexBuffer()) {
            LOG_ERROR("Failed to create vertex buffer");
            return false;
        }
        
        LOG_INFO("Metal renderer initialized");
        return true;
    }
    
    bool RenderCVPixelBuffer(CVPixelBufferRef pixel_buffer) {
        if (!pixel_buffer || !texture_cache_) return false;
        
        // 1. 锁定像素缓冲区
        CVPixelBufferLockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly);
        
        // 2. 创建Metal纹理
        CVMetalTextureRef y_texture_ref = nullptr;
        CVMetalTextureRef uv_texture_ref = nullptr;
        
        size_t width = CVPixelBufferGetWidth(pixel_buffer);
        size_t height = CVPixelBufferGetHeight(pixel_buffer);
        
        // Y平面
        CVMetalTextureCacheCreateTextureFromImage(
            nullptr, texture_cache_, pixel_buffer, nullptr,
            MTLPixelFormatR8Unorm, width, height, 0, &y_texture_ref);
        
        // UV平面
        CVMetalTextureCacheCreateTextureFromImage(
            nullptr, texture_cache_, pixel_buffer, nullptr,
            MTLPixelFormatRG8Unorm, width / 2, height / 2, 1, &uv_texture_ref);
        
        id<MTLTexture> y_texture = CVMetalTextureGetTexture(y_texture_ref);
        id<MTLTexture> uv_texture = CVMetalTextureGetTexture(uv_texture_ref);
        
        // 3. 获取可绘制纹理
        id<CAMetalDrawable> drawable = [metal_layer_ nextDrawable];
        if (!drawable) {
            CVBufferRelease(y_texture_ref);
            CVBufferRelease(uv_texture_ref);
            CVPixelBufferUnlockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly);
            return false;
        }
        
        // 4. 创建命令缓冲区
        id<MTLCommandBuffer> command_buffer = [command_queue_ commandBuffer];
        
        // 5. 创建渲染命令编码器
        MTLRenderPassDescriptor* pass_descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        pass_descriptor.colorAttachments[0].texture = drawable.texture;
        pass_descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass_descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass_descriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        
        id<MTLRenderCommandEncoder> encoder = [command_buffer renderCommandEncoderWithDescriptor:pass_descriptor];
        
        // 6. 设置渲染状态
        [encoder setRenderPipelineState:pipeline_state_];
        [encoder setVertexBuffer:vertex_buffer_ offset:0 atIndex:0];
        [encoder setFragmentTexture:y_texture atIndex:0];
        [encoder setFragmentTexture:uv_texture atIndex:1];
        
        // 7. 绘制
        [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip 
                    vertexStart:0 
                    vertexCount:4];
        
        [encoder endEncoding];
        
        // 8. 提交并显示
        [command_buffer presentDrawable:drawable];
        [command_buffer commit];
        
        // 9. 清理
        CVBufferRelease(y_texture_ref);
        CVBufferRelease(uv_texture_ref);
        CVPixelBufferUnlockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly);
        
        return true;
    }
    
    void Resize(int width, int height) {
        metal_layer_.drawableSize = CGSizeMake(width, height);
    }
    
    void Release() {
        if (texture_cache_) {
            CFRelease(texture_cache_);
            texture_cache_ = nullptr;
        }
        vertex_buffer_ = nil;
        pipeline_state_ = nil;
        command_queue_ = nil;
        device_ = nil;
    }
    
private:
    id<MTLDevice> device_;
    id<MTLCommandQueue> command_queue_;
    id<MTLRenderPipelineState> pipeline_state_;
    CAMetalLayer* metal_layer_;
    CVMetalTextureCacheRef texture_cache_;
    id<MTLBuffer> vertex_buffer_;
    
    CAMetalLayer* GetMetalLayer(void* native_view) {
        UIView* view = (__bridge UIView*)native_view;
        
        // 检查是否已有Metal层
        if ([view.layer isKindOfClass:[CAMetalLayer class]]) {
            return (CAMetalLayer*)view.layer;
        }
        
        // 创建新的Metal层
        CAMetalLayer* metal_layer = [CAMetalLayer layer];
        metal_layer.frame = view.bounds;
        [view.layer addSublayer:metal_layer];
        
        return metal_layer;
    }
    
    bool CreatePipeline() {
        // 加载着色器
        NSError* error = nil;
        id<MTLLibrary> library = [device_ newDefaultLibrary];
        if (!library) {
            LOG_ERROR("Failed to load default library");
            return false;
        }
        
        id<MTLFunction> vertex_func = [library newFunctionWithName:@"vertexShader"];
        id<MTLFunction> fragment_func = [library newFunctionWithName:@"fragmentShaderNV12"];
        
        if (!vertex_func || !fragment_func) {
            LOG_ERROR("Failed to load shader functions");
            return false;
        }
        
        // 创建渲染管线描述
        MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
        descriptor.vertexFunction = vertex_func;
        descriptor.fragmentFunction = fragment_func;
        descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        
        // 创建渲染管线状态
        pipeline_state_ = [device_ newRenderPipelineStateWithDescriptor:descriptor 
                                                                  error:&error];
        if (error) {
            LOG_ERROR("Failed to create pipeline state: %s", 
                     [[error localizedDescription] UTF8String]);
            return false;
        }
        
        return true;
    }
    
    bool CreateVertexBuffer() {
        // 顶点数据：位置和纹理坐标
        // 格式: position(x,y), texCoord(u,v)
        float vertices[] = {
            // 左上角
            -1.0f,  1.0f,  0.0f, 0.0f,
            // 右上角
             1.0f,  1.0f,  1.0f, 0.0f,
            // 左下角
            -1.0f, -1.0f,  0.0f, 1.0f,
            // 右下角
             1.0f, -1.0f,  1.0f, 1.0f,
        };
        
        vertex_buffer_ = [device_ newBufferWithBytes:vertices 
                                              length:sizeof(vertices) 
                                             options:MTLResourceStorageModeShared];
        
        return vertex_buffer_ != nil;
    }
};
```

### 8.5 网络流播放（HLS/RTMP）

```cpp
// 网络流播放器
class NetworkPlayer {
public:
    NetworkPlayer() 
        : format_ctx_(nullptr), 
          interrupt_requested_(false),
          buffer_time_(5000),      // 5秒缓冲
          buffer_size_(1024 * 1024) {  // 1MB缓冲
    }
    
    bool Open(const std::string& url) {
        // 1. 分配格式上下文
        format_ctx_ = avformat_alloc_context();
        if (!format_ctx_) {
            return false;
        }
        
        // 2. 设置打断回调
        format_ctx_->interrupt_callback.callback = InterruptCallback;
        format_ctx_->interrupt_callback.opaque = this;
        
        // 3. 设置选项
        AVDictionary* opts = nullptr;
        
        // 网络超时设置
        av_dict_set(&opts, "timeout", "10000000", 0);  // 10秒
        av_dict_set(&opts, "stimeout", "10000000", 0); // TCP连接超时
        
        // HLS特定设置
        if (url.find(".m3u8") != std::string::npos || 
            url.find("hls") != std::string::npos) {
            av_dict_set(&opts, "http_persistent", "0", 0);
            av_dict_set(&opts, "hls_seek_time", "0", 0);
            av_dict_set(&opts, "live_start_index", "-3", 0);
        }
        
        // RTMP特定设置
        if (url.find("rtmp") != std::string::npos) {
            av_dict_set(&opts, "rtmp_live", "live", 0);
            av_dict_set(&opts, "buffer", "0", 0);
        }
        
        // 缓冲设置
        av_dict_set(&opts, "buffer_size", std::to_string(buffer_size_).c_str(), 0);
        
        // 4. 打开输入
        int ret = avformat_open_input(&format_ctx_, url.c_str(), nullptr, &opts);
        av_dict_free(&opts);
        
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            LOG_ERROR("Failed to open %s: %s", url.c_str(), errbuf);
            return false;
        }
        
        // 5. 获取流信息
        ret = avformat_find_stream_info(format_ctx_, nullptr);
        if (ret < 0) {
            LOG_ERROR("Failed to find stream info");
            return false;
        }
        
        // 6. 查找视频和音频流
        for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
            AVStream* stream = format_ctx_->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index_ = i;
                video_stream_ = stream;
            } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_stream_index_ = i;
                audio_stream_ = stream;
            }
        }
        
        LOG_INFO("Network stream opened: %s", url.c_str());
        LOG_INFO("  Duration: %lld ms", format_ctx_->duration / 1000);
        LOG_INFO("  Bitrate: %lld bps", format_ctx_->bit_rate);
        
        return true;
    }
    
    // 读取线程
    void ReadThread() {
        AVPacket* packet = av_packet_alloc();
        
        while (!interrupt_requested_) {
            // 检查缓冲状态
            if (IsBufferFull()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // 读取包
            int ret = av_read_frame(format_ctx_, packet);
            if (ret < 0) {
                if (ret == AVERROR_EOF) {
                    LOG_INFO("End of stream");
                    break;
                } else if (ret == AVERROR(EAGAIN)) {
                    // 需要更多数据
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                } else {
                    char errbuf[256];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    LOG_ERROR("Read error: %s", errbuf);
                    break;
                }
            }
            
            // 分发包
            if (packet->stream_index == video_stream_index_) {
                video_packet_queue_.Push(packet);
            } else if (packet->stream_index == audio_stream_index_) {
                audio_packet_queue_.Push(packet);
            }
            
            packet = av_packet_alloc();
        }
        
        av_packet_free(&packet);
    }
    
    // 定位
    bool Seek(int64_t position_ms) {
        if (!format_ctx_) return false;
        
        int64_t target_pts = position_ms * 1000;  // 转换为微秒
        
        // 对于直播流，不支持定位
        if (format_ctx_->duration <= 0) {
            LOG_WARNING("Cannot seek in live stream");
            return false;
        }
        
        int ret = av_seek_frame(format_ctx_, -1, target_pts, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            LOG_ERROR("Seek failed: %d", ret);
            return false;
        }
        
        // 清空缓冲
        video_packet_queue_.Clear();
        audio_packet_queue_.Clear();
        
        return true;
    }
    
    // 获取缓冲状态
    BufferStatus GetBufferStatus() {
        BufferStatus status;
        status.video_packets = video_packet_queue_.Size();
        status.audio_packets = audio_packet_queue_.Size();
        status.is_full = IsBufferFull();
        status.is_low = IsBufferLow();
        
        // 计算缓冲时长
        if (video_stream_) {
            AVRational time_base = video_stream_->time_base;
            int64_t buffered_duration = 0;
            // 计算队列中包的总时长...
            status.buffered_duration_ms = buffered_duration * 1000 / 
                (time_base.num * time_base.den);
        }
        
        return status;
    }
    
    void Close() {
        interrupt_requested_ = true;
        
        if (format_ctx_) {
            avformat_close_input(&format_ctx_);
        }
    }
    
private:
    AVFormatContext* format_ctx_;
    int video_stream_index_;
    int audio_stream_index_;
    AVStream* video_stream_;
    AVStream* audio_stream_;
    
    std::atomic<bool> interrupt_requested_;
    
    // 缓冲设置
    int64_t buffer_time_;   // 缓冲时长（毫秒）
    int64_t buffer_size_;   // 缓冲大小（字节）
    
    // 包队列
    PacketQueue video_packet_queue_;
    PacketQueue audio_packet_queue_;
    
    static int InterruptCallback(void* ctx) {
        NetworkPlayer* player = static_cast<NetworkPlayer*>(ctx);
        return player->interrupt_requested_.load() ? 1 : 0;
    }
    
    bool IsBufferFull() {
        // 检查视频缓冲
        if (video_stream_index_ >= 0) {
            // 计算缓冲时长
            // 简化实现：检查队列大小
            if (video_packet_queue_.Size() > 100) {
                return true;
            }
        }
        return false;
    }
    
    bool IsBufferLow() {
        if (video_stream_index_ >= 0) {
            if (video_packet_queue_.Size() < 10) {
                return true;
            }
        }
        return false;
    }
};
```

---

## 9. 总结

### 9.1 技术方案要点

| 模块 | 推荐方案 | 备选方案 |
|------|----------|----------|
| FFmpeg版本 | 6.1.x LTS | 5.1.x |
| iOS/macOS硬解 | VideoToolbox | - |
| Android硬解 | MediaCodec | - |
| Windows硬解 | D3D11VA | DXVA2 |
| iOS/macOS渲染 | Metal | OpenGL(已弃用) |
| Android渲染 | OpenGL ES 3.0 | Vulkan |
| Windows渲染 | DirectX 11 | DirectX 12 |

### 9.2 实现优先级

1. **阶段一（本地播放）**: 软解 + 基础渲染
2. **阶段二（网络流）**: 添加HLS/RTMP支持
3. **阶段三（硬解）**: 各平台硬解码集成
4. **阶段四（编码）**: 录制功能实现
5. **阶段五（优化）**: 零拷贝、性能优化

### 9.3 注意事项

1. **内存管理**: 使用AVFrame/AVPacket池避免频繁分配
2. **线程安全**: 队列操作需要加锁保护
3. **错误处理**: 硬解码失败需要回退到软解
4. **平台差异**: 使用抽象层隔离平台代码
5. **性能监控**: 添加统计信息便于调优

---

*文档版本: 1.0*
*最后更新: 2024年*
