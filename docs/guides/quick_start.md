# Quick Start Guide

## Prerequisites

- **macOS**: Xcode 12+, CMake 3.16+, FFmpeg 6.1.x
- **iOS**: Xcode 12+, iOS 12.0+ deployment target
- **Android**: Android Studio, NDK r21+, API 21+
- **Windows**: Visual Studio 2019+, CMake 3.16+

## Build the SDK

### macOS

```bash
# Clone repository
git clone https://github.com/LouisHors/HorsAVSDK.git
cd HorsAVSDK

# Create build directory
mkdir -p build/macos && cd build/macos

# Configure with CMake
cmake ../.. -DCMAKE_BUILD_TYPE=Release \
    -DFFMPEG_ROOT=/path/to/ffmpeg

# Build
make -j$(sysctl -n hw.ncpu)
```

### iOS

```bash
mkdir -p build/ios && cd build/ios
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../cmake/ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DFFMPEG_ROOT=/path/to/ffmpeg-ios
make -j$(sysctl -n hw.ncpu)
```

### Android

```bash
mkdir -p build/android && cd build/android
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21
make -j$(nproc)
```

## Run Tests

```bash
cd build/macos
ctest --output-on-failure

# Run specific test
./tests/core/player_test
```

## Build Example Player

```bash
cd examples/macos_player
xcodebuild -project HorsAVPlayer.xcodeproj -scheme HorsAVPlayer
```

## Next Steps

- Read [SDK Architecture](../context/sdk_architecture_current.md)
- Check [Interface Design](../context/sdk_interface_design.md)
- Review [Build Guide](build_guide.md) for detailed build options
