# Development Guidelines

## Code Style

### C++ (Core Implementation)

- Follow [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with modifications
- Use **C++11/14** features (avoid C++17+ for compatibility)
- Use 4 spaces for indentation
- Maximum line length: 120 characters
- Header files use `.h` extension, implementation files use `.cpp`

**Naming Conventions:**

| Type | Convention | Example |
|------|------------|---------|
| Classes/Structs | PascalCase | `FFmpegDemuxer`, `PlayerConfig` |
| Interfaces | PascalCase with `I` prefix | `IPlayer`, `IRenderer` |
| Functions/Methods | PascalCase for public, camelCase for private | `Open()`, `processPacket()` |
| Variables | snake_case | `format_ctx_`, `is_playing_` |
| Member Variables | snake_case with `_` suffix | `codec_ctx_`, `buffer_size_` |
| Constants | UPPER_SNAKE_CASE or kPascalCase | `MAX_BUFFER_SIZE`, `kHardware` |

**Example Class:**

```cpp
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
    
    // Interface
    int Open(const std::string& url) override;
    void Close() override;
    
private:
    AVFormatContext* format_ctx_ = nullptr;
    std::string current_url_;
    std::atomic<bool> is_open_{false};
    mutable std::mutex mutex_;
};
```

### Objective-C (iOS/macOS)

- Follow Apple's [Coding Guidelines for Cocoa](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/CodingGuidelines/CodingGuidelines.html)
- Use 4 spaces for indentation
- Prefix class names with `AVSDK` (e.g., `AVSDKPlayer`)
- Use properties instead of ivars when possible
- Bridge C++ objects using Objective-C++ (.mm files)

### Swift (iOS/macOS)

- Follow [Swift API Design Guidelines](https://www.swift.org/documentation/api-design-guidelines/)
- Maximum line length: 120 characters
- Prefer `let` over `var`, avoid force unwrapping
- Use `guard` for early returns

### Java/Kotlin (Android)

- Follow [Android Kotlin Style Guide](https://developer.android.com/kotlin/style-guide)
- Use 4 spaces for indentation
- Package name: `com.avsdk.*`
- JNI method names: `Java_com_avsdk_<class>_<method>`

## Best Practices

### Memory Management

1. **Use smart pointers**: `std::unique_ptr`/`std::shared_ptr` instead of raw pointers
2. **Explicit constructors**: Use `explicit` for single-argument constructors
3. **Override**: Use `override` for virtual function overrides
4. **Null pointer**: Prefer `nullptr` over `NULL` or `0`

### FFmpeg Integration

1. **Always use smart pointers with custom deleters:**
```cpp
auto frame = std::shared_ptr<AVFrame>(av_frame_alloc(), 
    [](AVFrame* f) { av_frame_free(&f); });
```

2. **Check FFmpeg version compatibility:**
```cpp
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 0, 0)
    // Handle older API
#endif
```

3. **Initialize FFmpeg properly:**
```cpp
// At SDK initialization
avformat_network_init();

// At SDK shutdown
avformat_network_deinit();
```

### Error Handling

```cpp
ErrorCode result = player->Open(url);
if (result != ErrorCode::OK) {
    LOG_ERROR("Open failed: {}", GetErrorString(result));
    
    switch (result) {
        case ErrorCode::NetworkConnectFailed:
            // Retry logic
            break;
        case ErrorCode::CodecNotSupported:
            // Try alternative codec
            break;
        default:
            break;
    }
    return result;
}
```

## Before Starting Development

1. Read [SDK Architecture](../context/sdk_architecture_current.md)
2. Read relevant interface specs (C++ or Objective-C)
3. Check existing implementation docs for your area
4. Update documentation as you develop

## Documentation Maintenance

- **New feature**: Create design document in `docs/context/` or `docs/plans/`
- **Modify feature**: Update existing documentation
- **Bug fix**: Add known issues section if applicable
- **Milestone**: Update implementation status in `sdk_architecture_current.md`
