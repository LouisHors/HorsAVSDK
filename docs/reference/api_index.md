# API Index

## Core C++ API

### Player Interface

| Class | Header | Description |
|-------|--------|-------------|
| `IPlayer` | `avsdk/player.h` | Main player interface |
| `PlayerConfig` | `avsdk/player_config.h` | Player configuration |
| `PlayerState` | `avsdk/types.h` | Player state enumeration |

```cpp
#include <avsdk/player.h>

// Create player
auto player = IPlayer::Create();

// Configure
PlayerConfig config;
config.enable_hardware_decode = true;
player->Initialize(config);

// Open and play
player->Open("path/to/video.mp4");
player->Play();
```

### Types

| Type | Header | Description |
|------|--------|-------------|
| `VideoFrame` | `avsdk/types.h` | Video frame structure |
| `AudioFrame` | `avsdk/types.h` | Audio frame structure |
| `MediaInfo` | `avsdk/types.h` | Media metadata |
| `ErrorCode` | `avsdk/error.h` | Error codes |

### Encoder Interface

| Class | Header | Description |
|-------|--------|-------------|
| `IEncoder` | `avsdk/encoder.h` | Video encoder interface |
| `EncoderConfig` | `avsdk/encoder.h` | Encoder configuration |

## Objective-C/Swift API

### Player

| Class | Header | Description |
|-------|--------|-------------|
| `HorsAVPlayer` | `HorsAVPlayer.h` | Main player class |
| `HorsAVPlayerConfiguration` | `HorsAVPlayerConfig.h` | Configuration |
| `HorsAVPlayerDelegate` | `HorsAVPlayer.h` | Player delegate protocol |

```objc
@import HorsAVSDK;

// Create player
HorsAVPlayer *player = [[HorsAVPlayer alloc] init];
player.delegate = self;

// Open and play
NSError *error;
[player openURL:[NSURL fileURLWithPath:@"video.mp4"] error:&error];
[player play];

// Delegate methods
- (void)player:(HorsAVPlayer *)player 
    didChangeState:(HorsAVPlayerState)oldState 
         toState:(HorsAVPlayerState)newState {
    // Handle state change
}
```

### Renderer

| Class | Header | Description |
|-------|--------|-------------|
| `HorsAVPlayerRenderer` | `HorsAVRenderer.h` | Video renderer |

### Types

| Type | Header | Description |
|------|--------|-------------|
| `HorsAVPlayerState` | `HorsAVTypes.h` | Player state |
| `HorsAVLogLevel` | `HorsAVLogger.h` | Log level |

## Error Handling

### C++

```cpp
ErrorCode result = player->Open(url);
if (result != ErrorCode::OK) {
    std::string msg = GetErrorString(result);
    // Handle error
}
```

### Objective-C

```objc
NSError *error;
BOOL success = [player openURL:url error:&error];
if (!success) {
    NSLog(@"Error: %@", error.localizedDescription);
}
```

## Callbacks

### C++

```cpp
class MyCallback : public IPlayerCallback {
public:
    void OnStateChanged(PlayerState oldState, PlayerState newState) override;
    void OnPrepared(const MediaInfo& info) override;
    void OnError(const ErrorInfo& error) override;
    void OnProgress(Timestamp current, Timestamp duration) override;
};
```

### Objective-C

```objc
@protocol HorsAVPlayerDelegate <NSObject>
- (void)player:(HorsAVPlayer *)player 
didChangeState:(HorsAVPlayerState)oldState 
       toState:(HorsAVPlayerState)newState;

- (void)player:(HorsAVPlayer *)player 
didEncounterError:(NSError *)error;

- (void)player:(HorsAVPlayer *)player 
didUpdateProgress:(NSTimeInterval)currentTime 
       duration:(NSTimeInterval)duration;
@end
```

## Configuration

### PlayerConfig (C++)

```cpp
struct PlayerConfig {
    bool enable_hardware_decode = true;
    bool enable_audio = true;
    bool enable_video = true;
    BufferStrategy buffer_strategy = BufferStrategy::kBalanced;
    uint32_t max_buffer_size_ms = 5000;
};
```

### HorsAVPlayerConfiguration (Objective-C)

```objc
@interface HorsAVPlayerConfiguration : NSObject
@property (nonatomic) BOOL enableHardwareDecode;
@property (nonatomic) BOOL enableAudio;
@property (nonatomic) BOOL enableVideo;
@property (nonatomic) HorsAVBufferStrategy bufferStrategy;
@property (nonatomic) NSUInteger maxBufferSizeMs;
@end
```

## State Machine

```
Idle → Preparing → Prepared → Playing → Paused
                        ↓         ↓
                     Stopped ←───┘
                        ↓
                      Error
```

| State | Description |
|-------|-------------|
| `Idle` | Initial state |
| `Preparing` | Loading media |
| `Prepared` | Ready to play |
| `Playing` | Playback active |
| `Paused` | Paused |
| `Stopped` | Stopped, resources released |
| `Error` | Error occurred |

## Version Compatibility

| SDK Version | Minimum iOS | Minimum Android | FFmpeg |
|-------------|-------------|-----------------|--------|
| 1.0.x | 12.0 | API 21 | 6.1.x |

See individual header files for complete API documentation.
