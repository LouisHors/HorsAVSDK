# Error Codes Reference

## Error Code Categories

| Range | Category |
|-------|----------|
| `0xxx` | General errors |
| `1xxx` | Player errors |
| `2xxx` | Codec errors |
| `3xxx` | Network errors |
| `4xxx` | File errors |
| `5xxx` | Hardware errors |

## General Errors (0xxx)

| Code | Name | Description |
|------|------|-------------|
| `0000` | `OK` | Success |
| `0001` | `InvalidParameter` | Invalid input parameter |
| `0002` | `OutOfMemory` | Memory allocation failed |
| `0003` | `NotImplemented` | Feature not implemented |
| `0004` | `Unknown` | Unknown error |

## Player Errors (1xxx)

| Code | Name | Description |
|------|------|-------------|
| `1001` | `PlayerNotInitialized` | Player not initialized |
| `1002` | `PlayerAlreadyPlaying` | Player already in playing state |
| `1003` | `PlayerNotPlaying` | Player not in playing state |
| `1004` | `OpenFailed` | Failed to open media |
| `1005` | `SeekFailed` | Seek operation failed |
| `1006` | `InvalidState` | Invalid player state for operation |
| `1007` | `NoMedia` | No media loaded |

## Codec Errors (2xxx)

| Code | Name | Description |
|------|------|-------------|
| `2001` | `CodecNotFound` | Codec not found |
| `2002` | `CodecNotSupported` | Codec not supported |
| `2003` | `DecodeFailed` | Decoding failed |
| `2004` | `EncodeFailed` | Encoding failed |
| `2005` | `CodecInitFailed` | Codec initialization failed |
| `2006` | `HWDecodeFailed` | Hardware decoding failed |

## Network Errors (3xxx)

| Code | Name | Description |
|------|------|-------------|
| `3001` | `NetworkConnectFailed` | Failed to connect to server |
| `3002` | `NetworkTimeout` | Network timeout |
| `3003` | `HTTPError` | HTTP error response |
| `3004` | `InvalidURL` | Invalid URL format |
| `3005` | `NetworkDisconnected` | Network disconnected |

## File Errors (4xxx)

| Code | Name | Description |
|------|------|-------------|
| `4001` | `FileNotFound` | File not found |
| `4002` | `FileReadError` | File read error |
| `4003` | `FileWriteError` | File write error |
| `4004` | `InvalidFormat` | Invalid file format |
| `4005` | `PermissionDenied` | Permission denied |

## Hardware Errors (5xxx)

| Code | Name | Description |
|------|------|-------------|
| `5001` | `HWNotAvailable` | Hardware acceleration not available |
| `5002` | `HWInitFailed` | Hardware initialization failed |
| `5003` | `HWDecodeNotSupported` | Hardware decoding not supported for codec |
| `5004` | `DeviceLost` | Graphics device lost |

## Error Handling Example

```cpp
ErrorCode result = player->Open(url);
if (result != ErrorCode::OK) {
    LOG_ERROR("Open failed: {}", GetErrorString(result));
    
    switch (result) {
        case ErrorCode::NetworkConnectFailed:
            // Retry with fallback URL
            break;
        case ErrorCode::CodecNotSupported:
            // Try software decoder
            break;
        case ErrorCode::FileNotFound:
            // Show user-friendly error
            break;
        default:
            // Generic error handling
            break;
    }
}
```

## Objective-C Error Handling

```objc
NSError *error = nil;
BOOL success = [player openURL:url error:&error];
if (!success) {
    NSLog(@"Error: %@", error.localizedDescription);
    // Handle specific error codes
}
```
