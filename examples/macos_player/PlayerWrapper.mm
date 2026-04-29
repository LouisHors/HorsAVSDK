#import "PlayerWrapper.h"
#import "HorsAVPlayer.h"
#import "HorsAVPlayerConfig.h"

@interface PlayerWrapper () <HorsAVPlayerDelegate>
@property (nonatomic, strong) HorsAVPlayer *player;
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
    if (self.player) {
        return YES;
    }

    // Configure
    HorsAVPlayerConfiguration *config = [HorsAVPlayerConfiguration defaultConfiguration];
    config.enableHardwareDecoder = self.enableHardwareDecoder;

    switch (self.decoderMode) {
        case DecoderModeAuto:
            config.decoderMode = HorsAVDecoderModeAuto;
            break;
        case DecoderModeSoftware:
            config.decoderMode = HorsAVDecoderModeSoftware;
            break;
        case DecoderModeHardware:
            config.decoderMode = HorsAVDecoderModeHardware;
            break;
        case DecoderModeHardwareFirst:
            config.decoderMode = HorsAVDecoderModeHardwareFirst;
            break;
    }

    NSError *error;
    self.player = [[HorsAVPlayer alloc] initWithConfiguration:config error:&error];

    if (!self.player) {
        [self reportError:error];
        return NO;
    }

    self.player.delegate = self;
    self.state = DemoPlayerStateReady;
    return YES;
}

- (void)releasePlayer {
    self.player = nil;
    self.state = DemoPlayerStateIdle;
}

#pragma mark - Playback Control

- (BOOL)openFile:(NSString *)filePath {
    NSURL *url = [NSURL fileURLWithPath:filePath];
    return [self openURL:url];
}

- (BOOL)openURL:(NSURL *)url {
    if (!self.player) {
        if (![self initializePlayer]) {
            return NO;
        }
    }

    NSError *error;
    if (![self.player openURL:url error:&error]) {
        [self reportError:error];
        self.state = DemoPlayerStateError;
        [self notifyStateChanged];
        return NO;
    }

    return YES;
}

- (void)play {
    if (!self.player) return;

    [self.player play];
    // State will be updated via delegate
}

- (void)pause {
    if (!self.player) return;

    [self.player pause];
    // State will be updated via delegate
}

- (void)stop {
    if (!self.player) return;

    [self.player stop];
    _currentTime = 0;
    // State will be updated via delegate
}

- (void)seekTo:(NSTimeInterval)time {
    if (!self.player) return;

    [self.player seekToTime:time completionHandler:^(BOOL success, NSTimeInterval actualTime, NSError *error) {
        if (!success && error) {
            [self reportError:error];
        }
    }];
}

#pragma mark - Rendering

- (void)setRenderView:(NSView *)view {
    if (!self.player) {
        if (![self initializePlayer]) {
            return;
        }
    }

    [self.player setRenderView:view];
}

#pragma mark - HorsAVPlayerDelegate

- (void)player:(HorsAVPlayer *)player didPrepareMedia:(HorsAVMediaInfo *)info {
    self.duration = info.duration;
    self.videoSize = CGSizeMake(info.videoWidth, info.videoHeight);
    self.state = DemoPlayerStateReady;
    [self notifyStateChanged];
}

- (void)player:(HorsAVPlayer *)player didChangeState:(HorsAVPlayerState)oldState toState:(HorsAVPlayerState)newState {
    self.state = [self convertState:newState];
    [self notifyStateChanged];
}

- (void)player:(HorsAVPlayer *)player didEncounterError:(NSError *)error {
    self.state = DemoPlayerStateError;
    [self reportError:error];
    [self notifyStateChanged];
}

- (void)playerDidCompletePlayback:(HorsAVPlayer *)player {
    self.state = DemoPlayerStateReady;
    self.currentTime = 0;
    [self notifyStateChanged];
}

- (void)player:(HorsAVPlayer *)player didUpdateProgress:(NSTimeInterval)currentTime duration:(NSTimeInterval)duration {
    self.currentTime = currentTime;
    if (duration > 0) {
        self.duration = duration;
    }
    [self notifyProgressUpdate];
}

- (void)player:(HorsAVPlayer *)player didChangeBuffering:(BOOL)isBuffering percent:(NSInteger)percent {
    if (isBuffering) {
        self.state = DemoPlayerStateBuffering;
    } else {
        self.state = [self convertState:player.state];
    }
    [self notifyStateChanged];
}

#pragma mark - Private Methods

- (DemoPlayerState)convertState:(HorsAVPlayerState)state {
    switch (state) {
        case HorsAVPlayerStateIdle:
            return DemoPlayerStateIdle;
        case HorsAVPlayerStateStopped:
            return DemoPlayerStateReady;
        case HorsAVPlayerStatePlaying:
            return DemoPlayerStatePlaying;
        case HorsAVPlayerStatePaused:
            return DemoPlayerStatePaused;
        case HorsAVPlayerStateError:
            return DemoPlayerStateError;
        default:
            return DemoPlayerStateIdle;
    }
}

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

- (void)reportError:(NSError *)error {
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(playerWrapper:didEncounterError:)]) {
            [self.delegate playerWrapper:self didEncounterError:error];
        }
    });
}

@end
