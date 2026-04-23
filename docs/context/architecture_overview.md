# Architecture Overview

## Layered Architecture

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

## Key Modules

### 1. Demuxer
Media format parsing using FFmpeg's libavformat. Supports local files and network streams.

### 2. Decoder
Video/audio decoding with hardware acceleration fallback:
- **VideoToolbox** (iOS/macOS)
- **MediaCodec** (Android)
- **DXVA/D3D11VA** (Windows)
- **Software** (FFmpeg fallback)

### 3. Encoder
Real-time H264/H265 encoding with hardware acceleration support.

### 4. Renderer
Platform-specific video rendering:
- **Metal** (iOS/macOS)
- **OpenGL ES** (Android)
- **DirectX** (Windows)

### 5. Clock
Audio-video synchronization management using audio as master clock.

## Thread Model

| Thread | Responsibility |
|--------|----------------|
| **Main Thread** | UI interaction and SDK API calls |
| **Demuxer Thread** | Reads and parses media data |
| **Video Decode Thread** | Decodes video packets |
| **Audio Decode Thread** | Decodes and resamples audio |
| **Render Thread** | Video rendering with vsync |
| **Audio Play Thread** | Audio output and synchronization |
| **Clock Thread** | AV sync management |

## Data Flow

```
Input File/Stream
       ↓
[Demuxer] → Extract audio/video packets
       ↓
[Decoder] → Decode to raw frames
       ↓
[Renderer/Audio Output] → Display/Play
       ↓
[Clock] → Synchronize AV
```

## Platform Abstraction

All platform-specific code implements abstract interfaces:

```cpp
class IRenderer {
public:
    virtual int Initialize(void* native_window, int width, int height) = 0;
    virtual int RenderFrame(const AVFrame* frame) = 0;
    virtual void Release() = 0;
};
```

See [Architecture Patterns](../context/architecture_patterns.md) for design patterns.
