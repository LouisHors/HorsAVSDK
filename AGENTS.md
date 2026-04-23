# AGENTS.md - HorsAVSDK Project

## Project Overview

HorsAVSDK is a **cross-platform audio/video SDK** based on FFmpeg, providing playback, encoding, and real-time streaming across iOS/iPadOS, Android, macOS, and Windows platforms.

**Core Technology Stack:**
- **Language**: C++11/14 (core), Objective-C/Swift (iOS/macOS), Java/Kotlin (Android), C# (Windows)
- **Framework**: FFmpeg 6.1.x LTS
- **Platform APIs**: VideoToolbox/MediaCodec/DXVA for hardware acceleration, Metal/OpenGL ES/DirectX for rendering

**Development Phases:**
- Phase 1: Local file playback (macOS first) ✅
- Phase 2: Network streaming (RTMP, HLS, FLV)
- Phase 3: Real-time encoding and capture
- Phase 4: Data bypass callbacks

---

## Quick Links

### Documentation
| Document | Description |
|----------|-------------|
| [`docs/context/sdk_architecture_current.md`](docs/context/sdk_architecture_current.md) | Current SDK architecture (READ FIRST) |
| [`docs/context/sdk_interface_design.md`](docs/context/sdk_interface_design.md) | C++ interface specifications |
| [`docs/context/objc_interface_spec.md`](docs/context/objc_interface_spec.md) | Objective-C/Swift bindings |
| [`docs/context/player_impl_design.md`](docs/context/player_impl_design.md) | Player implementation details |
| [`docs/context/metal_rendering_strategy.md`](docs/context/metal_rendering_strategy.md) | macOS Metal rendering |
| [`docs/context/git_constraints.md`](docs/context/git_constraints.md) | Git workflow & commit conventions |

### Product Requirements
- [`docs/context/跨平台音视频SDK_PRD.md`](docs/context/跨平台音视频SDK_PRD.md) - Product requirements
- [`docs/context/media_sdk_architecture_design.md`](docs/context/media_sdk_architecture_design.md) - Initial architecture
- [`docs/context/ffmpeg_sdk_technical_solution.md`](docs/context/ffmpeg_sdk_technical_solution.md) - FFmpeg technical solution

---

## Architecture

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
1. **Demuxer**: Media format parsing
2. **Decoder**: Video/audio decoding with HW acceleration fallback
3. **Encoder**: Real-time H264/H265 encoding
4. **Renderer**: Platform-specific video rendering
5. **Clock**: Audio-video synchronization

### Thread Model
- **Main Thread**: UI interaction and SDK API calls
- **Demuxer Thread**: Reads and parses media data
- **Video Decode Thread**: Decodes video packets
- **Audio Decode Thread**: Decodes and resamples audio
- **Render Thread**: Video rendering with vsync
- **Audio Play Thread**: Audio output and synchronization
- **Clock Thread**: AV sync management

---

## Development Guidelines

### Before Starting
1. Read [`docs/context/sdk_architecture_current.md`](docs/context/sdk_architecture_current.md)
2. Read relevant interface specs (C++ or Objective-C)
3. Check existing implementation docs for your area

### Code Style
- **C++**: Google Style Guide, 4 spaces, 120 char limit, `snake_case_` for members
- **Objective-C**: Apple Cocoa Guidelines, `AVSDK` prefix
- **Swift**: Swift API Design Guidelines
- **Java/Kotlin**: Android Kotlin Style Guide

See detailed conventions: [`docs/context/code_style_guide.md`](docs/context/code_style_guide.md) *(create if needed)*

### Architecture Patterns
- Platform abstraction via interfaces (`IRenderer`, `IDecoder`)
- Factory pattern for hardware decoders
- Smart pointers with custom deleters for FFmpeg objects
- Memory pool for performance-critical paths
- Observer pattern for callbacks

See [`docs/context/architecture_patterns.md`](docs/context/architecture_patterns.md) *(create if needed)*

---

## Git Workflow

⚠️ **main branch is protected**. All changes must go through PR.

```bash
git checkout -b feature/my-feature
git commit -m "feat(player): add new feature"
git push -u origin feature/my-feature
gh pr create --title "feat: add new feature" --body "Description"
```

**Branch naming**: `feature/description`, `bugfix/description`, `hotfix/description`, `platform/feature`

**Commit format**: [Conventional Commits](https://www.conventionalcommits.org/) - `type(scope): description`

Full rules: [`docs/context/git_constraints.md`](docs/context/git_constraints.md)

---

## Build & Test

### Requirements
| Platform | Minimum | Build Tools |
|----------|---------|-------------|
| iOS | iOS 12.0+ | Xcode 12+, Metal |
| Android | API 21+ | NDK r21+, OpenGL ES 3.0 |
| macOS | 10.14+ | Xcode 12+, Metal |
| Windows | Windows 10 1803+ | VS 2019+, DirectX 11 |

### Build
```bash
mkdir -p build/macos && cd build/macos
cmake ../.. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### Test
```bash
cd build/macos
ctest --output-on-failure
```

---

## Performance Targets

| Metric | Target |
|--------|--------|
| First frame (local) | ≤ 500ms |
| First frame (network) | ≤ 2s |
| 1080p@30fps CPU | < 30% (iPhone 12) |
| Live latency | ≤ 3s |
| AV sync deviation | ≤ 40ms |
| Memory growth | ≤ 100MB |

---

## Error Codes

- `0xxx`: General errors
- `1xxx`: Player errors
- `2xxx`: Codec errors
- `3xxx`: Network errors
- `4xxx`: File errors
- `5xxx`: Hardware errors

---

## Security

- Never log sensitive URLs
- Validate external URLs before FFmpeg
- Sanitize file paths
- Use HTTPS for streams
- Handle untrusted media carefully
