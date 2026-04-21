#import "PlayerWrapper.h"

// C++ Headers
#include "player.h"
#include "player_config.h"
#include "error.h"
#include "renderer.h"
#include "types.h"
#include "metal_renderer.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <memory>
#include <string>

using namespace avsdk;

@interface PlayerWrapper () {
    std::shared_ptr<IPlayer> _player;
    std::unique_ptr<IRenderer> _renderer;
    DemoPlayerState _state;
    NSTimeInterval _currentTime;
    NSTimeInterval _duration;
    CGSize _videoSize;
}
@end

@implementation PlayerWrapper

- (instancetype)init {
    self = [super init];
    if (self) {
        _decoderMode = DecoderModeAuto;
        _enableHardwareDecoder = YES;
        _state = DemoPlayerStateIdle;
        _currentTime = 0;
        _duration = 0;
        _videoSize = CGSizeZero;
    }
    return self;
}

- (void)dealloc {
    [self releasePlayer];
}

#pragma mark - Lifecycle

- (BOOL)initializePlayer {
    if (_player) {
        return YES; // Already initialized
    }

    _player = CreatePlayer();
    if (!_player) {
        [self reportErrorWithCode:ErrorCode::Unknown message:@"Failed to create player"];
        return NO;
    }

    // Configure player
    PlayerConfig config;
    config.enable_hardware_decoder = _enableHardwareDecoder;
    config.buffer_time_ms = 1000; // 1 second buffer

    ErrorCode result = _player->Initialize(config);
    if (result != ErrorCode::OK) {
        [self reportErrorWithCode:result message:@"Failed to initialize player"];
        _player.reset();
        return NO;
    }

    _state = DemoPlayerStateIdle;
    return YES;
}

- (void)releasePlayer {
    if (_player) {
        _player->Stop();
        _player->Close();
        _player.reset();
    }

    if (_renderer) {
        _renderer->Release();
        _renderer.reset();
    }

    _state = DemoPlayerStateIdle;
    _currentTime = 0;
    _duration = 0;
    _videoSize = CGSizeZero;
}

#pragma mark - Playback Control

- (BOOL)openFile:(NSString *)filePath {
    if (!_player) {
        if (![self initializePlayer]) {
            return NO;
        }
    }

    std::string url = [filePath UTF8String];
    ErrorCode result = _player->Open(url);

    if (result != ErrorCode::OK) {
        [self reportErrorWithCode:result
                          message:[NSString stringWithFormat:@"Failed to open file: %@", filePath]];
        _state = DemoPlayerStateError;
        [self notifyStateChanged];
        return NO;
    }

    // Get media info
    MediaInfo info = _player->GetMediaInfo();
    _duration = info.duration_ms / 1000.0;
    _videoSize = CGSizeMake(info.video_width, info.video_height);

    _state = DemoPlayerStateReady;
    [self notifyStateChanged];

    return YES;
}

- (void)play {
    if (!_player) return;

    ErrorCode result = _player->Play();
    if (result == ErrorCode::OK) {
        _state = DemoPlayerStatePlaying;
        [self notifyStateChanged];
        [self startProgressTimer];
    } else {
        [self reportErrorWithCode:result message:@"Failed to play"];
    }
}

- (void)pause {
    if (!_player) return;

    ErrorCode result = _player->Pause();
    if (result == ErrorCode::OK) {
        _state = DemoPlayerStatePaused;
        [self notifyStateChanged];
    }
}

- (void)stop {
    if (!_player) return;

    _player->Stop();
    _state = DemoPlayerStateReady;
    _currentTime = 0;
    [self notifyStateChanged];
}

- (void)seekTo:(NSTimeInterval)time {
    if (!_player) return;

    // Convert seconds to milliseconds
    int64_t positionMs = static_cast<int64_t>(time * 1000);
    ErrorCode result = _player->Seek(positionMs);

    if (result != ErrorCode::OK) {
        [self reportErrorWithCode:result message:@"Failed to seek"];
    } else {
        _currentTime = time;
        [self notifyProgressUpdate];
    }
}

#pragma mark - Rendering

- (void)setRenderView:(MTKView *)view {
    if (!_player) {
        [self reportErrorWithCode:ErrorCode::NotInitialized message:@"Player not initialized"];
        return;
    }

    // Create Metal renderer
    _renderer = CreateMetalRenderer();
    if (!_renderer) {
        [self reportErrorWithCode:ErrorCode::HardwareNotAvailable message:@"Failed to create Metal renderer"];
        return;
    }

    // Initialize renderer with native window
    void* nativeWindow = (__bridge void*)view;

    // Set render view in player (renderer will be initialized when Open is called)
    _player->SetRenderView(nativeWindow);

    // Store renderer reference in player with custom deleter (PlayerWrapper owns it)
    _player->SetRenderer(std::shared_ptr<IRenderer>(_renderer.get(), [](IRenderer*) {
        // Custom deleter - do nothing since _renderer is owned by PlayerWrapper
    }));
}

#pragma mark - Private Methods

- (void)notifyStateChanged {
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(playerWrapper:stateChanged:)]) {
            [self.delegate playerWrapper:self stateChanged:self.state];
        }
    });
}

- (void)notifyProgressUpdate {
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(playerWrapper:didUpdateProgress:duration:)]) {
            [self.delegate playerWrapper:self didUpdateProgress:self.currentTime duration:self.duration];
        }
    });
}

- (void)reportErrorWithCode:(ErrorCode)code message:(NSString *)message {
    NSString *errorString = [NSString stringWithUTF8String:GetErrorString(code)];
    NSString *fullMessage = [NSString stringWithFormat:@"%@: %@", errorString, message];

    NSError *error = [NSError errorWithDomain:@"HorsAVPlayer"
                                         code:static_cast<NSInteger>(code)
                                     userInfo:@{NSLocalizedDescriptionKey: fullMessage}];

    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(playerWrapper:didEncounterError:)]) {
            [self.delegate playerWrapper:self didEncounterError:error];
        }
    });
}

- (void)startProgressTimer {
    // Progress updates will be triggered by the player callbacks
    // For now, poll the current position
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        while (self.state == DemoPlayerStatePlaying) {
            if (self->_player) {
                self->_currentTime = self->_player->GetCurrentPosition() / 1000.0;
                [self notifyProgressUpdate];
            }
            [NSThread sleepForTimeInterval:0.5];
        }
    });
}

#pragma mark - Property Accessors

- (DemoPlayerState)state {
    if (_player) {
        DemoPlayerState cppState = [self convertCPPState:_player->GetState()];
        // Only update if valid state transition
        if (cppState != DemoPlayerStateError || _state == DemoPlayerStateError) {
            return cppState;
        }
    }
    return _state;
}

- (NSTimeInterval)currentTime {
    if (_player) {
        return _player->GetCurrentPosition() / 1000.0;
    }
    return _currentTime;
}

- (NSTimeInterval)duration {
    if (_player) {
        return _player->GetDuration() / 1000.0;
    }
    return _duration;
}

- (CGSize)videoSize {
    if (_player) {
        MediaInfo info = _player->GetMediaInfo();
        return CGSizeMake(info.video_width, info.video_height);
    }
    return _videoSize;
}

#pragma mark - State Conversion

- (DemoPlayerState)convertCPPState:(avsdk::PlayerState)cppState {
    switch (cppState) {
        case avsdk::PlayerState::kIdle:
            return DemoPlayerStateIdle;
        case avsdk::PlayerState::kStopped:
            return DemoPlayerStateReady;
        case avsdk::PlayerState::kPlaying:
            return DemoPlayerStatePlaying;
        case avsdk::PlayerState::kPaused:
            return DemoPlayerStatePaused;
        case avsdk::PlayerState::kError:
            return DemoPlayerStateError;
        default:
            return DemoPlayerStateIdle;
    }
}

@end
