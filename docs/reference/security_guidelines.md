# Security Guidelines

## Input Validation

### URL Validation

Always validate URLs before passing to FFmpeg:

```cpp
bool IsValidMediaURL(const std::string& url) {
    // Check for valid scheme
    if (!StartsWith(url, "http://") && 
        !StartsWith(url, "https://") &&
        !StartsWith(url, "file://") &&
        !StartsWith(url, "/")) {
        return false;
    }
    
    // Prevent directory traversal
    if (url.find("..") != std::string::npos) {
        return false;
    }
    
    // Validate file extensions for local files
    if (IsLocalFile(url)) {
        return HasValidExtension(url, {".mp4", ".mov", ".mkv", ".avi"});
    }
    
    return true;
}
```

### File Path Sanitization

```cpp
std::string SanitizePath(const std::string& path) {
    // Remove null bytes
    std::string sanitized = path;
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\0'), 
                      sanitized.end());
    
    // Resolve to absolute path
    char resolved[PATH_MAX];
    if (realpath(sanitized.c_str(), resolved) != nullptr) {
        return std::string(resolved);
    }
    
    return "";
}
```

## Sensitive Data Handling

### URL Logging

Never log URLs with credentials:

```cpp
std::string MaskSensitiveURL(const std::string& url) {
    // Remove tokens from URLs
    std::string masked = url;
    
    // Mask query parameters that might contain tokens
    size_t token_pos = masked.find("token=");
    if (token_pos != std::string::npos) {
        size_t end = masked.find("&", token_pos);
        masked.replace(token_pos + 6, 
                      (end == std::string::npos) ? std::string::npos : end - token_pos - 6,
                      "***MASKED***");
    }
    
    return masked;
}

// Usage
LOG_INFO("Opening URL: {}", MaskSensitiveURL(url));
```

### Secure Storage

Store sensitive credentials securely:

```objc
// iOS/macOS: Use Keychain
- (void)storeCredential:(NSString *)credential forKey:(NSString *)key {
    NSDictionary *query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrAccount: key,
        (__bridge id)kSecValueData: [credential dataUsingEncoding:NSUTF8StringEncoding],
    };
    SecItemAdd((__bridge CFDictionaryRef)query, NULL);
}
```

## Network Security

### HTTPS Enforcement

Prefer HTTPS for network streams:

```cpp
bool ShouldUseHTTPS(const std::string& url) {
    if (StartsWith(url, "http://")) {
        LOG_WARNING("Insecure HTTP URL detected: {}", MaskSensitiveURL(url));
        return false;
    }
    return StartsWith(url, "https://");
}
```

### Certificate Validation

```cpp
// Configure FFmpeg for strict certificate validation
AVDictionary* options = nullptr;
av_dict_set(&options, "verify_ssl", "1", 0);
av_dict_set(&options, "cafile", "/path/to/ca-bundle.crt", 0);

int ret = avformat_open_input(&fmt_ctx, url.c_str(), nullptr, &options);
```

## Media File Security

### Untrusted Media Handling

Handle potentially malicious media files carefully:

```cpp
ErrorCode OpenMedia(const std::string& url) {
    // Set resource limits
    avformat_network_init();
    
    // Limit probe size to prevent DoS
    AVDictionary* options = nullptr;
    av_dict_set(&options, "probesize", "5000000", 0);  // 5MB max
    av_dict_set(&options, "analyzeduration", "5000000", 0);  // 5s max
    
    // Open with timeout
    int ret = avformat_open_input(&fmt_ctx, url.c_str(), nullptr, &options);
    if (ret < 0) {
        return ErrorCode::OpenFailed;
    }
    
    // Validate stream count (prevent resource exhaustion)
    if (fmt_ctx->nb_streams > 10) {
        LOG_ERROR("Suspicious: too many streams ({})", fmt_ctx->nb_streams);
        return ErrorCode::InvalidFormat;
    }
    
    return ErrorCode::OK;
}
```

### Buffer Overflow Prevention

```cpp
// Always check sizes before copying
if (packet->size > MAX_PACKET_SIZE) {
    LOG_ERROR("Oversized packet: {} > {}", packet->size, MAX_PACKET_SIZE);
    return ErrorCode::InvalidParameter;
}

// Use bounded string functions
char errbuf[AV_ERROR_MAX_STRING_SIZE];
av_strerror(ret, errbuf, sizeof(errbuf));  // FFmpeg handles bounds
```

## Memory Safety

### Integer Overflow Checks

```cpp
bool CheckSizeOverflow(size_t a, size_t b) {
    if (a > 0 && b > SIZE_MAX / a) {
        return false;  // Would overflow
    }
    return true;
}

// Usage
size_t buffer_size = width * height * 4;
if (!CheckSizeOverflow(width * height, 4)) {
    return ErrorCode::OutOfMemory;
}
```

### Safe Memory Allocation

```cpp
template<typename T>
std::unique_ptr<T[]> SafeAllocate(size_t count) {
    if (count > MAX_ALLOCATION_SIZE / sizeof(T)) {
        return nullptr;
    }
    return std::make_unique<T[]>(count);
}
```

## Platform-Specific Security

### iOS/macOS

- Use App Sandbox
- Enable Library Validation (`-library_validation`)
- Code sign all binaries
- Use `NSAllowsArbitraryLoads` carefully in ATS

### Android

- Minimize JNI attack surface
- Validate all JNI inputs
- Use `android:usesCleartextTraffic="false"`
- Enable ProGuard/R8 for release builds

### Windows

- Enable ASLR and DEP
- Use Control Flow Guard (CFG)
- Validate all COM interfaces

## Security Checklist

- [ ] All URLs validated before use
- [ ] No credentials in logs
- [ ] HTTPS preferred for network streams
- [ ] Input sizes checked before allocation
- [ ] Buffer bounds validated
- [ ] Resource limits enforced
- [ ] Untrusted media handled carefully
- [ ] Platform security features enabled
- [ ] Dependencies kept up to date
- [ ] Security patches applied promptly

## Reporting Security Issues

Report security vulnerabilities privately:
- Email: [security contact]
- Do not open public issues for security bugs
- Include detailed reproduction steps
- Allow time for fix before disclosure
