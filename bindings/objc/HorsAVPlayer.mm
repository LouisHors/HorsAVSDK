#import "HorsAVPlayer.h"
#import "HorsAVPlayerConfig.h"
#import "HorsAVPlayerInternal.h"
#import "HorsAVCallbackBridge.h"
#import "HorsAVDataBypass.h"
#import "HorsAVError.h"

#include "avsdk/player.h"
#include "avsdk/player_config.h"
#include "avsdk/types.h"
#include "avsdk/data_bypass.h"
#include "platform/macos/metal_renderer.h"
#include <memory>
#include <string>

using namespace avsdk;

// Forward declarations for private initializers defined in HorsAVTypes.mm
@interface HorsAVAudioTrackInfo ()
- (instancetype)initWithTrackIndex:(NSInteger)trackIndex
                          language:(NSString *)language
                             title:(NSString *)title
                        sampleRate:(NSInteger)sampleRate
                          channels:(NSInteger)channels;
@end

@interface HorsAVMediaInfo ()
- (instancetype)initWithURL:(NSString *)url
                     format:(NSString *)format
                   duration:(NSTimeInterval)duration
                 videoWidth:(NSInteger)width
                videoHeight:(NSInteger)height
            videoFrameRate:(double)frameRate
           audioSampleRate:(NSInteger)sampleRate
              audioChannels:(NSInteger)channels
                   hasVideo:(BOOL)hasVideo
                   hasAudio:(BOOL)hasAudio
                 audioTracks:(NSArray<HorsAVAudioTrackInfo *> *)audioTracks;
@end

// MARK: - Private Interface

@interface HorsAVPlayer () {
    std::shared_ptr<IPlayer> _cppPlayer;
}

@property (nonatomic, strong) HorsAVCallbackBridge *callbackBridge;
@property (nonatomic, strong) HorsAVMediaInfo *cachedMediaInfo;
@property (nonatomic, readwrite) HorsAVPlayerState state;
@property (nonatomic, readwrite) NSTimeInterval currentTime;
@property (nonatomic, readwrite) NSTimeInterval duration;
@property (nonatomic, readwrite) CGSize videoSize;
@property (nonatomic, readwrite, getter=isBuffering) BOOL buffering;
@property (nonatomic, readwrite) NSInteger bufferingPercent;
@property (nonatomic, readwrite) BOOL isReady;

@end

// MARK: - Implementation

@implementation HorsAVPlayer

#pragma mark - Initialization

- (instancetype)init {
    return [self initWithConfiguration:nil error:nil];
}

- (nullable instancetype)initWithConfiguration:(nullable HorsAVPlayerConfiguration *)configuration
                                         error:(NSError **)error {
    self = [super init];
    if (self) {
        _state = HorsAVPlayerStateIdle;
        _currentTime = 0;
        _duration = 0;
        _videoSize = CGSizeZero;
        _buffering = NO;
        _bufferingPercent = 0;
        _isReady = NO;

        // Create C++ player
        _cppPlayer = CreatePlayer();
        if (!_cppPlayer) {
            if (error) {
                *error = [NSError errorWithDomain:HorsAVErrorDomain
                                             code:HorsAVErrorCodeUnknown
                                         userInfo:@{NSLocalizedDescriptionKey: @"Failed to create player"}];
            }
            return nil;
        }

        // Configure player
        PlayerConfig config;
        if (configuration) {
            config.enable_hardware_decoder = configuration.enableHardwareDecoder;
            config.buffer_time_ms = static_cast<int>(configuration.bufferTime * 1000);
        }

        ErrorCode result = _cppPlayer->Initialize(config);
        if (result != ErrorCode::OK) {
            if (error) {
                NSString *message = HorsAVGetErrorDescription(static_cast<HorsAVErrorCode>(result)) ?: @"Unknown error";
                *error = [NSError errorWithDomain:HorsAVErrorDomain
                                             code:static_cast<HorsAVErrorCode>(result)
                                         userInfo:@{NSLocalizedDescriptionKey: message}];
            }
            _cppPlayer.reset();
            return nil;
        }

        // Initialize callback bridge
        self.callbackBridge = [[HorsAVCallbackBridge alloc] initWithPlayer:self];
        _cppPlayer->SetCallback(self.callbackBridge.playerCallback.get());

        _state = HorsAVPlayerStateStopped;
    }
    return self;
}

- (void)dealloc {
    [self close];
    _cppPlayer.reset();
}

#pragma mark - C++ Player Access

- (std::shared_ptr<avsdk::IPlayer>)cppPlayer {
    return _cppPlayer;
}

#pragma mark - Playback Control

- (BOOL)openURL:(NSURL *)url error:(NSError **)error {
    if (!_cppPlayer) {
        if (error) {
            *error = [NSError errorWithDomain:HorsAVErrorDomain
                                         code:HorsAVErrorCodeNotInitialized
                                     userInfo:@{NSLocalizedDescriptionKey: @"Player not initialized"}];
        }
        return NO;
    }

    // For file URLs, use path property which handles percent-decoding correctly
    std::string urlString;
    if (url.isFileURL) {
        urlString = std::string([url.path UTF8String]);
    } else {
        urlString = std::string([url.absoluteString UTF8String]);
    }

    ErrorCode result = _cppPlayer->Open(urlString);

    if (result != ErrorCode::OK) {
        if (error) {
            NSString *message = HorsAVGetErrorDescription(static_cast<HorsAVErrorCode>(result)) ?: @"Failed to open";
            *error = [NSError errorWithDomain:HorsAVErrorDomain
                                         code:static_cast<HorsAVErrorCode>(result)
                                     userInfo:@{NSLocalizedDescriptionKey: message}];
        }
        _state = HorsAVPlayerStateError;
        return NO;
    }

    // Cache media info
    MediaInfo info = _cppPlayer->GetMediaInfo();
    _cachedMediaInfo = [self convertMediaInfo:info url:url];
    _duration = info.duration_ms / 1000.0;
    _videoSize = CGSizeMake(info.video_width, info.video_height);
    _isReady = YES;

    // Notify delegate
    if ([self.delegate respondsToSelector:@selector(player:didPrepareMedia:)]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self.delegate player:self didPrepareMedia:self.cachedMediaInfo];
        });
    }

    return YES;
}

- (void)play {
    if (!_cppPlayer) return;

    ErrorCode result = _cppPlayer->Play();
    if (result == ErrorCode::OK) {
        _state = HorsAVPlayerStatePlaying;
        [self notifyStateChangedFrom:HorsAVPlayerStateStopped to:HorsAVPlayerStatePlaying];
    }
}

- (void)pause {
    if (!_cppPlayer) return;

    ErrorCode result = _cppPlayer->Pause();
    if (result == ErrorCode::OK) {
        HorsAVPlayerState oldState = _state;
        _state = HorsAVPlayerStatePaused;
        [self notifyStateChangedFrom:oldState to:HorsAVPlayerStatePaused];
    }
}

- (void)stop {
    if (!_cppPlayer) return;

    _cppPlayer->Stop();
    _state = HorsAVPlayerStateStopped;
    _currentTime = 0;
    _isReady = NO;
    [self notifyStateChangedFrom:HorsAVPlayerStatePlaying to:HorsAVPlayerStateStopped];
}

- (void)seekToTime:(NSTimeInterval)time
 completionHandler:(nullable void (^)(BOOL success, NSTimeInterval actualTime, NSError *_Nullable error))completionHandler {
    if (!_cppPlayer) {
        if (completionHandler) {
            completionHandler(NO, 0, [NSError errorWithDomain:HorsAVErrorDomain
                                                         code:HorsAVErrorCodeNotInitialized
                                                     userInfo:@{NSLocalizedDescriptionKey: @"Player not initialized"}]);
        }
        return;
    }

    int64_t positionMs = static_cast<int64_t>(time * 1000);
    ErrorCode result = _cppPlayer->Seek(positionMs);

    if (completionHandler) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (result == ErrorCode::OK) {
                self.currentTime = time;
                completionHandler(YES, time, nil);
            } else {
                NSString *message = HorsAVGetErrorDescription(static_cast<HorsAVErrorCode>(result)) ?: @"Seek failed";
                NSError *error = [NSError errorWithDomain:HorsAVErrorDomain
                                                     code:static_cast<HorsAVErrorCode>(result)
                                                 userInfo:@{NSLocalizedDescriptionKey: message}];
                completionHandler(NO, self.currentTime, error);
            }
        });
    }
}

- (void)close {
    if (_cppPlayer) {
        _cppPlayer->Close();
    }
    _state = HorsAVPlayerStateStopped;
    _isReady = NO;
    _cachedMediaInfo = nil;
}

#pragma mark - Rendering

- (void)setRenderView:(NSView *)view {
    if (!_cppPlayer) return;

    void *nativeWindow = (__bridge void *)view;
    _cppPlayer->SetRenderView(nativeWindow);

    // Create and attach the platform-specific Metal renderer
    auto renderer = CreateMetalRenderer();
    if (renderer) {
        _cppPlayer->SetRenderer(std::move(renderer));
    }
}

- (void)setRenderMode:(HorsAVRenderMode)mode {
    // Implementation depends on renderer configuration
    // This would set the render mode through the C++ interface
}

#pragma mark - Data Bypass

- (void)setDataBypassConfiguration:(HorsAVDataBypassConfiguration *)configuration {
    if (!_cppPlayer) return;

    // Enable/disable callbacks based on configuration
    _cppPlayer->EnableVideoFrameCallback(configuration.enableVideoFrames);
    _cppPlayer->EnableAudioFrameCallback(configuration.enableAudioFrames);
}

#pragma mark - Audio Tracks

- (NSArray<HorsAVAudioTrackInfo *> *)audioTracks {
    if (!_cppPlayer) return nil;
    auto tracks = _cppPlayer->GetAudioTracks();
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:tracks.size()];
    for (size_t i = 0; i < tracks.size(); i++) {
        const auto &track = tracks[i];
        HorsAVAudioTrackInfo *info = [[HorsAVAudioTrackInfo alloc] initWithTrackIndex:track.stream_index
                                                                             language:@(track.language.c_str())
                                                                                title:@(track.title.c_str())
                                                                           sampleRate:track.sample_rate
                                                                             channels:track.channels];
        [result addObject:info];
    }
    return result;
}

- (NSInteger)selectedAudioTrack {
    if (!_cppPlayer) return 0;
    // C++ side doesn't expose selected index directly; we track it or return 0
    return 0;
}

- (BOOL)selectAudioTrack:(NSInteger)trackIndex error:(NSError **)error {
    if (!_cppPlayer) {
        if (error) {
            *error = [NSError errorWithDomain:HorsAVErrorDomain
                                         code:HorsAVErrorCodeNotInitialized
                                     userInfo:@{NSLocalizedDescriptionKey: @"Player not initialized"}];
        }
        return NO;
    }
    ErrorCode result = _cppPlayer->SelectAudioTrack(static_cast<int>(trackIndex));
    if (result != ErrorCode::OK) {
        if (error) {
            NSString *message = HorsAVGetErrorDescription(static_cast<HorsAVErrorCode>(result)) ?: @"Failed to select audio track";
            *error = [NSError errorWithDomain:HorsAVErrorDomain
                                         code:static_cast<HorsAVErrorCode>(result)
                                     userInfo:@{NSLocalizedDescriptionKey: message}];
        }
        return NO;
    }
    return YES;
}

#pragma mark - Private Methods

- (HorsAVMediaInfo *)convertMediaInfo:(const MediaInfo &)info url:(NSURL *)url {
    NSMutableArray *audioTracks = [NSMutableArray arrayWithCapacity:info.audio_tracks.size()];
    for (const auto &track : info.audio_tracks) {
        HorsAVAudioTrackInfo *trackInfo = [[HorsAVAudioTrackInfo alloc] initWithTrackIndex:track.stream_index
                                                                                  language:@(track.language.c_str())
                                                                                     title:@(track.title.c_str())
                                                                                sampleRate:track.sample_rate
                                                                                  channels:track.channels];
        [audioTracks addObject:trackInfo];
    }
    return [[HorsAVMediaInfo alloc] initWithURL:url.absoluteString
                                         format:@(info.format.c_str())
                                       duration:info.duration_ms / 1000.0
                                     videoWidth:info.video_width
                                    videoHeight:info.video_height
                                 videoFrameRate:info.video_framerate
                                audioSampleRate:info.audio_sample_rate
                                   audioChannels:info.audio_channels
                                        hasVideo:info.has_video
                                        hasAudio:info.has_audio
                                      audioTracks:audioTracks];
}

- (void)notifyStateChangedFrom:(HorsAVPlayerState)oldState to:(HorsAVPlayerState)newState {
    if ([self.delegate respondsToSelector:@selector(player:didChangeState:toState:)]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self.delegate player:self didChangeState:oldState toState:newState];
        });
    }
}

@end

