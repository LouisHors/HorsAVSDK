# AGENTS.md - HorsAVSDK Project

## Project Overview

HorsAVSDK is a **cross-platform audio/video SDK** based on FFmpeg, providing playback, encoding, and real-time streaming capabilities across iOS/iPadOS, Android, macOS, and Windows platforms.

**Core Technology Stack:**
- **Language**: C++11/14 (core), Objective-C/Swift (iOS/macOS), Java/Kotlin (Android), C# (Windows)
- **Framework**: FFmpeg 6.1.x LTS (libavcodec, libavformat, libswscale)
- **Platform APIs**: VideoToolbox/MediaCodec/DXVA for hardware acceleration, Metal/OpenGL ES/DirectX for rendering

**Development Phases:**
- Phase 1: Local file playback (macOS first as validation platform) ✅
  - Video playback: FFmpeg decoding + Metal rendering ✅
  - Audio playback: FFmpeg decoding + AudioUnit rendering ✅
  - Basic AV sync ✅
  - Objective-C/Swift bindings ✅
- Phase 2: Network streaming (RTMP, HLS, FLV over HTTP)
- Phase 3: Real-time encoding and capture
- Phase 4: Data bypass callbacks for all pipeline stages

---

## Architecture Overview

The SDK follows a **layered architecture**:

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

---

## Code Style Guide

### C++ (Core Implementation)

- Follow [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with modifications
- Use **C++11/14** features (avoid C++17+ for compatibility)
- Use 4 spaces for indentation
- Maximum line length: 120 characters
- Header files use `.h` extension, implementation files use `.cpp`
- Class names use PascalCase with suffix (e.g., `FFmpegDemuxer`, `MetalRenderer`)
- Private members end with underscore (e.g., `codec_ctx_`, `is_initialized_`)
- Use `std::unique_ptr`/`std::shared_ptr` instead of raw pointers
- Use `explicit` for single-argument constructors
- Use `override` for virtual function overrides
- Prefer `nullptr` over `NULL` or `0`

```cpp
// Example class declaration
class FFmpegDemuxer : public IDemuxer {
public:
    explicit FFmpegDemuxer(const DemuxerConfig& config);
    ~FFmpegDemuxer() override;
    
    // Disable copy
    FFmpegDemuxer(const FFmpegDemuxer&) = delete;
    FFmpegDemuxer& operator=(const FFmpegDemuxer&) = delete;
    
    // Enable move
    FFmpegDemuxer(FFmpegDemuxer&&) noexcept = default;
    FFmpegDemuxer& operator=(FFmpegDemuxer&&) noexcept = default;
    
    // IDemuxer interface
    int Open(const std::string& url) override;
    void Close() override;
    
private:
    AVFormatContext* format_ctx_ = nullptr;
    std::string current_url_;
    std::atomic<bool> is_open_{false};
    mutable std::mutex mutex_;
};
```

### Objective-C (iOS/macOS Platform Layer)

- Follow Apple's [Coding Guidelines for Cocoa](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/CodingGuidelines/CodingGuidelines.html)
- Use 4 spaces for indentation
- Prefix class names with `AVSDK` (e.g., `AVSDKPlayer`, `AVSDKVideoRenderer`)
- Use properties instead of ivars when possible
- Bridge C++ objects using opaque pointers or Objective-C++ (.mm files)
- Handle memory management carefully when bridging ARC and non-ARC code

### Swift (iOS/macOS Application Layer)

- Follow [Swift API Design Guidelines](https://www.swift.org/documentation/api-design-guidelines/)
- Maximum line length: 120 characters
- Prefer `let` over `var`, avoid force unwrapping
- Use `guard` for early returns
- Document public APIs with Swift documentation comments

### Java/Kotlin (Android Platform Layer)

- Follow [Android Kotlin Style Guide](https://developer.android.com/kotlin/style-guide)
- Use 4 spaces for indentation
- Package name: `com.avsdk.*`
- Class names use PascalCase (e.g., `AVSDKPlayer`, `MediaCodecDecoder`)
- JNI method names follow pattern: `Java_com_avsdk_<class>_<method>`
- Handle JNI references carefully (local vs global refs)

---

## Architecture Patterns

### 1. Platform Abstraction

All platform-specific code is behind abstract interfaces:

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

### 2. Factory Pattern for Hardware Decoders

```cpp
class HWDecoderFactory {
public:
    static std::unique_ptr<IDecoder> CreateDecoder(HWAccelType type);
    static HWAccelType GetOptimalDecoder(AVCodecID codec_id);
};
```

### 3. Reference Counting for FFmpeg Objects

```cpp
using AVFramePtr = std::shared_ptr<AVFrame>;
using AVPacketPtr = std::shared_ptr<AVPacket>;

// Custom deleters ensure proper FFmpeg cleanup
auto frame = AVFramePtr(av_frame_alloc(), 
    [](AVFrame* f) { av_frame_free(&f); });
```

### 4. Memory Pool for Performance-Critical Paths

```cpp
class MemoryPool {
public:
    static void* Allocate(size_t size);
    static void Free(void* ptr, size_t size);
};
```

### 5. Observer Pattern for Callbacks

```cpp
class IPlayerCallback {
public:
    virtual void OnStateChanged(PlayerState oldState, PlayerState newState) = 0;
    virtual void OnPrepared(const MediaInfo& info) = 0;
    virtual void OnError(const ErrorInfo& error) = 0;
    // ... other callbacks
};
```

---

## Naming Conventions

### C++

| Type | Convention | Example |
|------|------------|---------|
| Classes/Structs | PascalCase | `FFmpegDemuxer`, `PlayerConfig` |
| Interfaces | PascalCase with `I` prefix | `IPlayer`, `IRenderer` |
| Functions/Methods | PascalCase for public, camelCase for private | `Open()`, `processPacket()` |
| Variables | snake_case | `format_ctx_`, `is_playing_` |
| Member Variables | snake_case with `_` suffix | `codec_ctx_`, `buffer_size_` |
| Constants/Enums | UPPER_SNAKE_CASE or kPascalCase | `MAX_BUFFER_SIZE`, `kHardware` |
| Template Parameters | PascalCase | `typename T`, `typename Allocator` |
| Macros | UPPER_SNAKE_CASE with project prefix | `AVSDK_LOG_ERROR`, `CHECK_NULL` |

### File Naming

- C++ headers: `.h`
- C++ implementation: `.cpp`
- Objective-C++ implementation: `.mm`
- Platform implementations: `<platform>_<module>.cpp` (e.g., `ios_video_renderer.mm`)
- Test files: `<module>_test.cpp`

---

## FFmpeg Integration Guidelines

### FFmpeg Object Management

1. **Always use smart pointers with custom deleters:**
```cpp
// Correct
auto frame = std::shared_ptr<AVFrame>(av_frame_alloc(), 
    [](AVFrame* f) { av_frame_free(&f); });

// Incorrect - memory leak risk
AVFrame* frame = av_frame_alloc();
```

2. **Check FFmpeg version compatibility:**
```cpp
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 0, 0)
    // Handle older API
#endif
```

3. **Properly initialize and uninitialize FFmpeg:**
```cpp
// At SDK initialization
avformat_network_init();

// At SDK shutdown
avformat_network_deinit();
```

### FFmpeg Error Handling

```cpp
int ret = avcodec_send_packet(codec_ctx_, packet);
if (ret < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
    LOG_ERROR("avcodec_send_packet failed: {}", errbuf);
    return ErrorCode::CodecDecodeFailed;
}
```

---

## Hardware Acceleration Guidelines

### VideoToolbox (iOS/macOS)

- Supports H264, H265, ProRes
- Outputs CVPixelBuffer for zero-copy rendering
- Always implement fallback to software decode
- Handle `VTDecompressionSession` errors gracefully

### MediaCodec (Android)

- Supports H264, H265, VP8, VP9
- Requires Surface for zero-copy path
- API 21+ required
- Handle codec configuration changes properly

### D3D11VA (Windows)

- Supports H264, H265, VP9
- Requires Direct3D 11 device
- Outputs D3D11 texture
- Handle device lost scenarios

---

## Error Handling Standards

### Error Code Categories

- `0xxx`: General errors (memory, parameters)
- `1xxx`: Player errors (open, seek, state)
- `2xxx`: Codec errors (not found, decode/encode failed)
- `3xxx`: Network errors (connect, timeout, HTTP)
- `4xxx`: File errors (not found, read/write)
- `5xxx`: Hardware errors (not available, init failed)

### Error Handling Pattern

```cpp
ErrorCode result = player->Open(url);
if (result != ErrorCode::OK) {
    LOG_ERROR("Open failed: {}", GetErrorString(result));
    
    // Handle specific errors
    switch (result) {
        case ErrorCode::NetworkConnectFailed:
            // Retry logic
            break;
        case ErrorCode::CodecNotSupported:
            // Try alternative codec
            break;
        default:
            // Generic error handling
            break;
    }
    return result;
}
```

---

## Testing Requirements

### Unit Tests

- Use **googletest** framework
- Minimum 80% code coverage for core modules
- Test both success and failure paths
- Mock FFmpeg dependencies where appropriate
- Name tests: `<Module>Test.<Scenario>` (e.g., `DecoderTest.HardwareFallback`)

```cpp
TEST(FFmpegDemuxerTest, OpenLocalFile) {
    auto demuxer = std::make_unique<FFmpegDemuxer>();
    ASSERT_EQ(demuxer->Open("test.mp4"), ErrorCode::OK);
    EXPECT_GT(demuxer->GetDuration(), 0);
}
```

### Platform-Specific Tests

- iOS/macOS: XCTest for platform adapters
- Android: JUnit for Java wrappers, gtest for native code
- Windows: Visual Studio Test Framework

### Performance Tests

- 1080p@30fps playback CPU usage < 30%
- Memory growth ≤ 100MB during playback
- First frame ≤ 500ms (local), ≤ 2s (network)
- AV sync deviation ≤ 40ms

---

## Documentation

### Knowledge Base Path
**Primary documentation location**: `./docs`

All project documentation, design decisions, and task tracking are stored in the `docs/` directory using the Horspowers unified documentation system.

### Documentation Structure
- `docs/plans/` - Design documents and implementation plans
- `docs/active/` - Active task and bug tracking documents
- `docs/archive/` - Completed/archived documents
- `docs/context/` - Context documents and reference materials (PRDs, specs)

### Key Reference Documents

#### 产品需求与架构
- `docs/context/跨平台音视频SDK_PRD.md` - 产品需求文档
- `docs/context/media_sdk_architecture_design.md` - 初始架构设计文档
- `docs/context/ffmpeg_sdk_technical_solution.md` - FFmpeg 技术方案
- `docs/context/跨平台音视频SDK完整技术方案.md` - 完整技术方案

#### 当前实现文档 (v1.0 里程碑)
- `docs/context/sdk_architecture_current.md` - 当前 SDK 架构（必读）
- `docs/context/sdk_interface_design.md` - SDK 接口设计规范
- `docs/context/player_impl_design.md` - PlayerImpl 实现详解
- `docs/context/metal_rendering_strategy.md` - macOS Metal 渲染策略
- `docs/objc_interface_spec.md` - Objective-C/Swift 接口规范（已实现 ✅）

#### 开发前必读
1. **新功能开发**: 先阅读 `sdk_architecture_current.md` 了解当前结构
2. **接口变更**: 参考 `sdk_interface_design.md` 确保接口一致性
3. **macOS 渲染**: 参考 `metal_rendering_strategy.md` 了解渲染管线
4. **播放器逻辑**: 参考 `player_impl_design.md` 了解播放流程

### Documentation Conventions
- Use Markdown format for all documentation
- Include frontmatter with metadata (type, status, created date)
- Keep documents focused and concise
- Update status as work progresses
- Archive completed documents regularly

---

## Development Workflow

### 开发前必读

在开展任何新功能开发之前，必须完成以下步骤：

1. **阅读架构文档**: 
   - 阅读 `docs/context/sdk_architecture_current.md` 了解当前 SDK 结构和已实现功能
   - 了解当前限制和已知问题

2. **阅读接口设计**:
   - 阅读 `docs/context/sdk_interface_design.md` 了解 C++ 公共接口规范
   - 阅读 `docs/objc_interface_spec.md` 了解 Objective-C/Swift 接口规范
   - 确保新功能与现有接口兼容

3. **阅读相关实现**:
   - 如果是播放器功能，阅读 `docs/context/player_impl_design.md`
   - 如果是 macOS 渲染功能，阅读 `docs/context/metal_rendering_strategy.md`

4. **更新文档**:
   - 开发完成后，更新相关文档以反映新的实现
   - 在文档中添加变更历史和版本号

### 文档维护规范

- **创建新功能**: 创建设计文档到 `docs/context/` 或 `docs/plans/`
- **修改现有功能**: 更新对应的现有文档
- **修复 bug**: 在相关文档中添加已知问题章节
- **里程碑**: 在 `sdk_architecture_current.md` 中更新实现状态表格

---

## Build & CI

### Build System

- **Primary**: CMake for all platforms
- **Secondary**: Xcode projects for iOS/macOS development
- **Android**: Gradle + CMake with Android NDK
- **Windows**: Visual Studio + CMake

### Platform Requirements

| Platform | Minimum Version | Build Requirements |
|----------|-----------------|-------------------|
| iOS | iOS 12.0+ | Xcode 12+, Metal support |
| iPadOS | iPadOS 12.0+ | Xcode 12+, Metal support |
| Android | API 21 (Android 5.0) | NDK r21+, OpenGL ES 3.0 |
| macOS | macOS 10.14+ | Xcode 12+, Metal support |
| Windows | Windows 10 1803+ | Visual Studio 2019+, DirectX 11 |

### FFmpeg Dependencies

Required FFmpeg libraries:
- `libavcodec` - Codec interface
- `libavformat` - Container format support
- `libavutil` - Utility functions
- `libswscale` - Software scaling
- `libswresample` - Audio resampling

Build FFmpeg with:
```bash
--enable-gpl --enable-version3 --enable-nonfree
--enable-libx264 --enable-libx265
--enable-videotoolbox  # macOS/iOS
--enable-mediacodec    # Android
--enable-dxva2 --enable-d3d11va  # Windows
```

### Code Style Enforcement

- Use clang-format for C++ code
- Configuration file: `.clang-format` (Google style based)
- CI checks formatting on pull requests

---

## Git Workflow

### Branch Protection (Enforced)

⚠️ **IMPORTANT**: `main` 分支受保护，**禁止直接推送**。

所有代码变更必须通过 Pull Request 流程：

```bash
# 1. 创建功能分支
git checkout -b feature/my-feature

# 2. 开发并提交（遵循 conventional commits）
git commit -m "feat(player): add new feature"

# 3. 推送并创建 PR
git push -u origin feature/my-feature
gh pr create --title "feat: add new feature" --body "Description"

# 4. 等待审查通过（至少 1 个 approval）
# 5. 合并到 main
```

### Branch Naming

| 类型 | 格式 | 示例 |
|------|------|------|
| 功能开发 | `feature/<description>` | `feature/phase1-local-playback` |
| Bug 修复 | `bugfix/<description>` | `bugfix/decoder-memory-leak` |
| 紧急修复 | `hotfix/<description>` | `hotfix/ios-crash` |
| 平台特定 | `feature/<platform>/<feature>` | `feature/ios/metal-renderer` |
| 文档更新 | `docs/<description>` | `docs/api-reference` |
| 重构 | `refactor/<description>` | `refactor/player-state-machine` |

### Commit Message Format

遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

```
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

**类型**: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `build`, `ci`, `perf`

**示例**:
- `feat: add VideoToolbox hardware decoder`
- `fix(audio): resolve buffer underrun issue`
- `docs(api): update Objective-C interface spec`
- `perf(rendering): optimize zero-copy path for iOS`

### Git Hooks (本地保护)

仓库配置了以下 Git hooks：

- **pre-commit**: 代码格式检查、阻止 .DS_Store、大文件检测
- **commit-msg**: 提交信息格式验证
- **pre-push**: 阻止直接推送到 main 分支

### 完整规范

详见: [`docs/context/git_constraints.md`](docs/context/git_constraints.md)

---

## Zero-Copy Implementation Guidelines

The SDK implements zero-copy paths where possible to minimize CPU overhead:

```
Traditional (with copy):
Decoder(GPU) → CPU memory → GPU texture → Display

Zero-copy:
Decoder(GPU) → Shared GPU texture → Display
```

### Platform-Specific Zero-Copy Paths

- **iOS/macOS**: VideoToolbox → CVPixelBuffer → Metal texture
- **Android**: MediaCodec → Surface → OpenGL ES external texture
- **Windows**: D3D11VA → D3D11 texture

### Requirements for Zero-Copy

1. Hardware decoder must output GPU-accessible memory
2. Renderer must support importing external textures
3. Pixel formats must be compatible (NV12/YUV420P)
4. Proper synchronization between decoder and renderer

---

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| First frame (local) | ≤ 500ms | Cold start |
| First frame (network) | ≤ 2s | Including buffering |
| 1080p@30fps CPU | < 30% | iPhone 12 equivalent |
| Live latency | ≤ 3s | RTMP/HLS end-to-end |
| AV sync deviation | ≤ 40ms | Human perception threshold |
| Memory growth | ≤ 100MB | During playback |
| Continuous playback | > 24h | No memory leaks |

---

## Security Considerations

- Never log sensitive URLs (may contain tokens)
- Validate all external URLs before passing to FFmpeg
- Sanitize file paths to prevent directory traversal
- Use HTTPS for network streams when possible
- Handle untrusted media files carefully (buffer overflows)
