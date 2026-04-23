# Architecture Patterns

## 1. Platform Abstraction

All platform-specific code is behind abstract interfaces, enabling cross-platform support with platform-specific optimizations.

```cpp
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual int Initialize(void* native_window, int width, int height) = 0;
    virtual int RenderFrame(const AVFrame* frame) = 0;
    virtual void Release() = 0;
};

// Platform implementations
class MetalRenderer : public IRenderer { ... };
class GLESRenderer : public IRenderer { ... };
class D3DRenderer : public IRenderer { ... };
```

**Benefits:**
- Clean separation of concerns
- Easy to add new platforms
- Testable with mock implementations

## 2. Factory Pattern

Hardware decoders are created via factory pattern for runtime selection:

```cpp
class HWDecoderFactory {
public:
    static std::unique_ptr<IDecoder> CreateDecoder(HWAccelType type);
    static HWAccelType GetOptimalDecoder(AVCodecID codec_id);
};

// Usage
auto decoder = HWDecoderFactory::CreateDecoder(HWAccelType::kVideoToolbox);
```

## 3. Reference Counting for FFmpeg Objects

FFmpeg objects are wrapped with reference counting for automatic memory management:

```cpp
using AVFramePtr = std::shared_ptr<AVFrame>;
using AVPacketPtr = std::shared_ptr<AVPacket>;

// Custom deleters ensure proper FFmpeg cleanup
auto frame = AVFramePtr(av_frame_alloc(), 
    [](AVFrame* f) { av_frame_free(&f); });
```

## 4. Memory Pool

Memory pool reduces allocation overhead for video frames:

```cpp
class MemoryPool {
public:
    static void* Allocate(size_t size);
    static void Free(void* ptr, size_t size);
};

// Usage
void* buffer = MemoryPool::Allocate(frame_size);
// ... use buffer ...
MemoryPool::Free(buffer, frame_size);
```

## 5. Observer Pattern

Callbacks are implemented using observer pattern:

```cpp
class IPlayerCallback {
public:
    virtual void OnStateChanged(PlayerState oldState, PlayerState newState) = 0;
    virtual void OnPrepared(const MediaInfo& info) = 0;
    virtual void OnError(const ErrorInfo& error) = 0;
    virtual void OnProgress(Timestamp current, Timestamp duration) = 0;
};
```

## 6. Circular Buffer (Producer-Consumer)

Audio playback uses circular buffer for flow control:

```cpp
template<typename T>
class RingBuffer {
public:
    bool Write(const T* data, size_t count);
    bool Read(T* data, size_t count);
    size_t Available() const;
    size_t Used() const;
};
```

**Use case:** Producer (decoder) writes audio data, Consumer (AudioUnit) reads at its own pace.

## 7. State Machine

Player uses state machine for lifecycle management:

```
Idle → Preparing → Prepared → Playing → Paused
                        ↓         ↓
                     Stopped ←───┘
                        ↓
                      Error
```

## 8. Zero-Copy Rendering

Minimize CPU overhead by avoiding memory copies:

```
Traditional:
Decoder(GPU) → CPU memory → GPU texture → Display

Zero-copy:
Decoder(GPU) → Shared GPU texture → Display
```

**Platform paths:**
- **iOS/macOS**: VideoToolbox → CVPixelBuffer → Metal texture
- **Android**: MediaCodec → Surface → OpenGL ES external texture
- **Windows**: D3D11VA → D3D11 texture
