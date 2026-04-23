# Build Guide

## Prerequisites

### macOS
- Xcode 12+ with command line tools
- CMake 3.16+
- FFmpeg 6.1.x (build from source or use prebuilt)

### iOS
- Xcode 12+
- CMake 3.16+
- FFmpeg iOS build

### Android
- Android Studio
- Android NDK r21+
- CMake 3.16+ (bundled with Android SDK)

### Windows
- Visual Studio 2019+
- CMake 3.16+
- FFmpeg Windows build

## Build FFmpeg

### macOS

```bash
# Download FFmpeg 6.1.x
wget https://ffmpeg.org/releases/ffmpeg-6.1.1.tar.xz
tar xf ffmpeg-6.1.1.tar.xz
cd ffmpeg-6.1.1

# Configure
./configure \
    --prefix=/usr/local/ffmpeg \
    --enable-gpl \
    --enable-version3 \
    --enable-nonfree \
    --enable-libx264 \
    --enable-libx265 \
    --enable-videotoolbox \
    --disable-static \
    --enable-shared

# Build
make -j$(sysctl -n hw.ncpu)
make install
```

### iOS (cross-compile)

Use [ffmpeg-kit](https://github.com/arthenica/ffmpeg-kit) or build manually with iOS toolchain.

## Build HorsAVSDK

### macOS

```bash
mkdir -p build/macos && cd build/macos
cmake ../.. \
    -DCMAKE_BUILD_TYPE=Release \
    -DFFMPEG_ROOT=/usr/local/ffmpeg
make -j$(sysctl -n hw.ncpu)
```

### iOS

```bash
mkdir -p build/ios && cd build/ios
cmake ../.. \
    -DCMAKE_TOOLCHAIN_FILE=../../cmake/ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DFFMPEG_ROOT=/path/to/ffmpeg-ios
make -j$(sysctl -n hw.ncpu)
```

### Android

```bash
mkdir -p build/android && cd build/android
cmake ../.. \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DFFMPEG_ROOT=/path/to/ffmpeg-android
make -j$(nproc)
```

### Windows (Visual Studio)

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64 `
    -DFFMPEG_ROOT=C:\ffmpeg
cmake --build . --config Release
```

## CMake Options

| Option | Description | Default |
|--------|-------------|---------|
| `CMAKE_BUILD_TYPE` | Build type (Debug/Release) | Release |
| `FFMPEG_ROOT` | FFmpeg installation path | - |
| `BUILD_TESTS` | Build unit tests | ON |
| `BUILD_EXAMPLES` | Build example applications | ON |
| `ENABLE_METAL` | Enable Metal renderer (macOS/iOS) | ON |
| `ENABLE_VIDEOBOX` | Enable VideoToolbox decoder | ON |

## Run Tests

```bash
cd build/macos

# Run all tests
ctest --output-on-failure

# Run specific test
./tests/core/player_test

# Run with verbose output
./tests/core/player_test --gtest_filter=PlayerTest.* --v=2
```

## Build Examples

### macOS Player

```bash
cd examples/macos_player
xcodebuild -project HorsAVPlayer.xcodeproj \
    -scheme HorsAVPlayer \
    -configuration Release \
    build
```

### Simple Player

```bash
cd build/macos
make simple_player
./examples/simple_player/simple_player /path/to/video.mp4
```

## Troubleshooting

### FFmpeg not found

```bash
# Set FFmpeg path explicitly
cmake .. -DFFMPEG_ROOT=/usr/local/ffmpeg
```

### Metal not available

Ensure Xcode is installed and Metal framework is available:
```bash
xcrun --sdk macosx --show-sdk-path
```

### Android NDK not found

Set NDK path:
```bash
export NDK=/Users/$(whoami)/Library/Android/sdk/ndk/21.0.6113669
```

## CI/CD

See `.github/workflows/` for GitHub Actions configuration.
