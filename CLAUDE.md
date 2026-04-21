# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HorsAVSDK is a cross-platform audio/video SDK based on FFmpeg. It provides playback, encoding, and real-time streaming capabilities across iOS/iPadOS, Android, macOS, and Windows platforms.

**Core Technology Stack:**
- FFmpeg 6.1.x LTS as the media processing engine
- C++11/14 for core implementation
- Platform-specific adapters: VideoToolbox (Apple), MediaCodec (Android), DXVA/D3D11VA (Windows)

**Development Strategy:**
- Phase 1: Local file playback (macOS first as validation platform) ✅
  - Video playback with FFmpeg decoding and Metal rendering ✅
  - Audio playback with FFmpeg decoding and AudioUnit rendering ✅
  - Basic AV sync ✅
- Phase 2: Network streaming (RTMP, HLS, FLV over HTTP)
- Phase 3: Real-time encoding and capture
- Phase 4: Data bypass callbacks for all pipeline stages

## Architecture Overview

The SDK follows a layered architecture:

```
Application Layer (iOS/Android/macOS/Windows)
         ↓
Platform Adapter Layer (Metal/OpenGL ES/DirectX/VideoToolbox/MediaCodec)
         ↓
Core Layer (C++) - Demuxer, Decoder, Encoder, Muxer, Renderer, Clock
         ↓
FFmpeg Layer (libavcodec, libavformat, libswscale)
         ↓
Infrastructure Layer (ThreadPool, MemoryPool, Logger)
```

### Key Modules

1. **Demuxer**: Media format parsing using FFmpeg's libavformat
2. **Decoder**: Video/audio decoding with hardware acceleration fallback
3. **Encoder**: Real-time H264/H265 encoding with hardware support
4. **Renderer**: Platform-specific video rendering (Metal/OpenGL ES/DirectX)
5. **Clock**: Audio-video synchronization management

### Thread Model

- **Main Thread**: UI interaction and SDK API calls
- **Demuxer Thread**: Reads and parses media data
- **Video Decode Thread**: Decodes video packets
- **Audio Decode Thread**: Decodes and resamples audio
- **Render Thread**: Video rendering with vsync
- **Audio Play Thread**: Audio output and synchronization
- **Clock Thread**: AV sync management

## Project Structure

```
media_sdk/
├── include/avsdk/          # Public C++ headers
│   ├── player.h            # IPlayer interface
│   ├── player_config.h     # PlayerConfig struct
│   ├── encoder.h           # IEncoder interface
│   ├── types.h             # Common types (VideoFrame, AudioFrame)
│   └── error.h             # ErrorCode definitions
├── src/
│   ├── core/               # Core implementation
│   │   ├── demuxer/        # Demuxer module
│   │   ├── decoder/        # Decoder (software & hardware)
│   │   ├── encoder/        # Encoder module
│   │   ├── renderer/       # Renderer abstraction
│   │   └── clock/          # AV sync clock
│   ├── platform/           # Platform implementations
│   │   ├── ios/            # iOS VideoToolbox/Metal
│   │   ├── android/        # Android MediaCodec/OpenGL ES
│   │   ├── macos/          # macOS Metal/VideoToolbox
│   │   └── windows/        # Windows DirectX/DXVA
│   ├── infrastructure/     # Utilities
│   │   ├── thread_pool/    # Thread pool implementation
│   │   ├── memory_pool/    # Memory pool for AVFrames
│   │   └── logger/         # Logging system
│   └── ffmpeg/             # FFmpeg integration layer
├── bindings/               # Language bindings
│   ├── objc/               # Objective-C wrapper
│   ├── java/               # Java/JNI wrapper
│   └── csharp/             # C# wrapper (Windows)
├── tests/                  # Test suites
│   ├── unit_tests/         # Unit tests (googletest)
│   ├── integration_tests/  # Integration tests
│   └── performance_tests/  # Performance benchmarks
└── examples/               # Demo applications
    ├── simple_player/      # Basic playback demo
    └── encoder_demo/       # Encoding demo
```

## Common Development Commands

### Building the SDK

**macOS (using CMake):**
```bash
mkdir -p build/macos && cd build/macos
cmake ../.. -DCMAKE_BUILD_TYPE=Release \
    -DFFMPEG_ROOT=/path/to/ffmpeg \
    -DPLATFORM=macOS
make -j$(sysctl -n hw.ncpu)
```

**iOS:**
```bash
mkdir -p build/ios && cd build/ios
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../cmake/ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DFFMPEG_ROOT=/path/to/ffmpeg-ios
make -j$(sysctl -n hw.ncpu)
```

**Android (using NDK):**
```bash
mkdir -p build/android && cd build/android
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21
make -j$(nproc)
```

### Running Tests

```bash
# Run all tests
cd build/macos
ctest --output-on-failure

# Run specific test
./tests/unit_tests/avsdk_unit_tests --gtest_filter=DecoderTest.*

# Run with verbose output
./tests/unit_tests/avsdk_unit_tests --gtest_filter=DecoderTest.* --v=2
```

### Code Style & Linting

The project uses clang-format for C++ code formatting:

```bash
# Format all source files
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Check formatting (CI)
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror
```

## Key Design Patterns

### 1. Platform Abstraction

All platform-specific code is behind abstract interfaces:

```cpp
// IRenderer is implemented per-platform
class IRenderer {
public:
    virtual int Initialize(void* native_window, int width, int height) = 0;
    virtual int RenderFrame(const AVFrame* frame) = 0;
    virtual void Release() = 0;
};

// macOS implementation uses Metal
class MetalRenderer : public IRenderer { ... };

// Android implementation uses OpenGL ES
class GLESRenderer : public IRenderer { ... };
```

### 2. Hardware Decoder Factory

Hardware decoders are created via factory pattern:

```cpp
class HWDecoderFactory {
public:
    static std::unique_ptr<IDecoder> CreateDecoder(HWAccelType type);
    static HWAccelType GetOptimalDecoder(AVCodecID codec_id);
};

// Usage
auto decoder = HWDecoderFactory::CreateDecoder(HWAccelType::kVideoToolbox);
```

### 3. Reference Counting for FFmpeg Objects

FFmpeg objects are wrapped with reference counting:

```cpp
using AVFramePtr = std::shared_ptr<AVFrame>;
using AVPacketPtr = std::shared_ptr<AVPacket>;

// Custom deleters ensure proper FFmpeg cleanup
auto frame = AVFramePtr(av_frame_alloc(), [](AVFrame* f) { av_frame_free(&f); });
```

### 4. Memory Pool for Performance-Critical Paths

```cpp
// Memory pool reduces allocation overhead for video frames
class MemoryPool {
public:
    static void* Allocate(size_t size);
    static void Free(void* ptr, size_t size);
};
```

## Important Implementation Details

### Hardware Acceleration

**VideoToolbox (iOS/macOS):**
- Supports H264, H265, ProRes
- Outputs CVPixelBuffer for zero-copy rendering
- Falls back to software decode on failure

**MediaCodec (Android):**
- Supports H264, H265, VP8, VP9
- Requires Surface for zero-copy path
- API 21+ required

**D3D11VA (Windows):**
- Supports H264, H265, VP9
- Requires Direct3D 11 device
- Outputs D3D11 texture

### Zero-Copy Rendering

The SDK implements zero-copy paths where possible:

```
Traditional (with copy):
Decoder(GPU) → CPU memory → GPU texture → Display

Zero-copy:
Decoder(GPU) → Shared GPU texture → Display
```

Platforms supporting zero-copy:
- iOS/macOS: VideoToolbox → Metal texture
- Android: MediaCodec → Surface (OpenGL ES external texture)
- Windows: D3D11VA → D3D11 texture

### Audio-Video Synchronization

The SDK uses audio as the master clock by default:

```cpp
class ClockManager {
    void UpdateAudioClock(double pts);  // Master
    void UpdateVideoClock(double pts);  // Slave
    double GetSyncDiff() const;         // AV diff
    int64_t GetWaitTime(double pts) const;  // How long to wait
};
```

### Buffer Strategy

Three buffer strategies are supported:

| Strategy | Buffer Time | Use Case |
|----------|-------------|----------|
| LowLatency | 300ms | Live streaming |
| Smooth | 5s | VOD playback |
| Balanced | 1s | General purpose |

## Error Handling

Error codes are categorized:

- `0xxx`: General errors (memory, parameters)
- `1xxx`: Player errors (open, seek, state)
- `2xxx`: Codec errors (not found, decode/encode failed)
- `3xxx`: Network errors (connect, timeout, HTTP)
- `4xxx`: File errors (not found, read/write)
- `5xxx`: Hardware errors (not available, init failed)

Example:
```cpp
ErrorCode result = player->Open(url);
if (result != ErrorCode::OK) {
    LOG_ERROR("Open failed: {}", GetErrorString(result));
    // Handle specific errors
    if (result == ErrorCode::NetworkConnectFailed) {
        // Retry logic
    }
}
```

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| First frame (local) | ≤ 500ms | Cold start |
| First frame (network) | ≤ 2s | Including buffering |
| 1080p@30fps CPU | < 30% | iPhone 12 equivalent |
| Live latency | ≤ 3s | RTMP/HLS end-to-end |
| AV sync deviation | ≤ 40ms | Human perception threshold |
| Memory growth | ≤ 100MB | During playback |

## Documentation Location

Project documentation is stored in `./docs/` using the Horspowers unified documentation system:

- `docs/plans/` - Design documents and implementation plans
- `docs/active/` - Active task and bug tracking
- `docs/archive/` - Completed/archived documents
- `docs/context/` - Reference materials and PRDs

Key reference documents:
- `docs/context/跨平台音视频SDK_PRD.md` - Product requirements
- `docs/context/media_sdk_architecture_design.md` - Architecture details
- `docs/context/av_sdk_interface_design.md` - API specifications
