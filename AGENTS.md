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

## Documentation Index

### Getting Started
| Document | Description |
|----------|-------------|
| [`docs/context/sdk_architecture_current.md`](docs/context/sdk_architecture_current.md) | Current SDK architecture (READ FIRST) |
| [`docs/context/sdk_interface_design.md`](docs/context/sdk_interface_design.md) | C++ interface specifications |
| [`docs/context/objc_interface_spec.md`](docs/context/objc_interface_spec.md) | Objective-C/Swift bindings |
| [`docs/guides/quick_start.md`](docs/guides/quick_start.md) | Quick start guide *(create)* |

### Architecture & Design
| Document | Description |
|----------|-------------|
| [`docs/context/architecture_overview.md`](docs/context/architecture_overview.md) | Architecture overview, key modules & thread model *(create)* |
| [`docs/context/architecture_patterns.md`](docs/context/architecture_patterns.md) | Design patterns: platform abstraction, factories, memory management *(create)* |
| [`docs/context/player_impl_design.md`](docs/context/player_impl_design.md) | Player implementation details |
| [`docs/context/metal_rendering_strategy.md`](docs/context/metal_rendering_strategy.md) | macOS Metal rendering |

### Development
| Document | Description |
|----------|-------------|
| [`docs/context/development_guidelines.md`](docs/context/development_guidelines.md) | Code style, patterns & best practices *(create)* |
| [`docs/context/git_constraints.md`](docs/context/git_constraints.md) | Git workflow, branch naming & commit conventions |
| [`docs/guides/build_guide.md`](docs/guides/build_guide.md) | Build & test instructions *(create)* |
| [`docs/guides/contributing.md`](docs/guides/contributing.md) | Contribution guidelines *(create)* |

### Reference
| Document | Description |
|----------|-------------|
| [`docs/reference/error_codes.md`](docs/reference/error_codes.md) | Error code reference *(create)* |
| [`docs/reference/performance_targets.md`](docs/reference/performance_targets.md) | Performance benchmarks & targets *(create)* |
| [`docs/reference/security_guidelines.md`](docs/reference/security_guidelines.md) | Security best practices *(create)* |
| [`docs/reference/api_index.md`](docs/reference/api_index.md) | API index *(create)* |

### Product Requirements
| Document | Description |
|----------|-------------|
| [`docs/context/跨平台音视频SDK_PRD.md`](docs/context/跨平台音视频SDK_PRD.md) | Product requirements |
| [`docs/context/media_sdk_architecture_design.md`](docs/context/media_sdk_architecture_design.md) | Initial architecture design |
| [`docs/context/ffmpeg_sdk_technical_solution.md`](docs/context/ffmpeg_sdk_technical_solution.md) | FFmpeg technical solution |

---

## Quick Reference

### Common Commands
```bash
# Build
mkdir -p build/macos && cd build/macos
cmake ../.. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)

# Test
cd build/macos
ctest --output-on-failure

# Git workflow
git checkout -b feature/my-feature
git commit -m "feat(player): add new feature"
git push -u origin feature/my-feature
gh pr create --title "feat: add new feature" --body "Description"
```

### Platform Requirements
| Platform | Minimum | Build Tools |
|----------|---------|-------------|
| iOS | iOS 12.0+ | Xcode 12+, Metal |
| Android | API 21+ | NDK r21+, OpenGL ES 3.0 |
| macOS | 10.14+ | Xcode 12+, Metal |
| Windows | Windows 10 1803+ | VS 2019+, DirectX 11 |

---

## Repository Structure

```
.
├── include/avsdk/          # Public C++ headers
├── src/
│   ├── core/               # Core implementation
│   ├── platform/           # Platform adapters
│   ├── infrastructure/     # Utilities
│   └── ffmpeg/             # FFmpeg integration
├── bindings/               # Language bindings
│   ├── objc/               # Objective-C/Swift
│   ├── java/               # Java/JNI
│   └── csharp/             # C#
├── tests/                  # Test suites
├── examples/               # Demo applications
└── docs/                   # Documentation
    ├── context/            # Architecture & design docs
    ├── guides/             # How-to guides
    └── reference/          # API & reference docs
```

---

## Quick Links

- **GitHub**: https://github.com/LouisHors/HorsAVSDK
- **Issues**: https://github.com/LouisHors/HorsAVSDK/issues
- **Pull Requests**: https://github.com/LouisHors/HorsAVSDK/pulls
