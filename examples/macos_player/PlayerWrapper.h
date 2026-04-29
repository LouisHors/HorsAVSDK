#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <MetalKit/MetalKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, DemoPlayerState) {
    DemoPlayerStateIdle,
    DemoPlayerStateReady,
    DemoPlayerStateBuffering,
    DemoPlayerStatePlaying,
    DemoPlayerStatePaused,
    DemoPlayerStateError
};

typedef NS_ENUM(NSInteger, DecoderMode) {
    DecoderModeAuto,
    DecoderModeSoftware,
    DecoderModeHardware,
    DecoderModeHardwareFirst
};

@class PlayerWrapper;

@protocol PlayerWrapperDelegate <NSObject>
@optional
- (void)playerWrapper:(PlayerWrapper *)wrapper stateChanged:(DemoPlayerState)state;
- (void)playerWrapper:(PlayerWrapper *)wrapper didUpdateProgress:(NSTimeInterval)currentTime duration:(NSTimeInterval)duration;
- (void)playerWrapper:(PlayerWrapper *)wrapper didEncounterError:(NSError *)error;
@end

@interface PlayerWrapper : NSObject

// Delegate
@property (nonatomic, weak, nullable) id<PlayerWrapperDelegate> delegate;

// Properties (readwrite internally)
@property (nonatomic, readwrite) DemoPlayerState state;
@property (nonatomic, readwrite) NSTimeInterval currentTime;
@property (nonatomic, readwrite) NSTimeInterval duration;
@property (nonatomic, readwrite) CGSize videoSize;

// Config properties
@property (nonatomic, assign) DecoderMode decoderMode;
@property (nonatomic, assign) BOOL enableHardwareDecoder;

// Lifecycle
- (BOOL)initializePlayer;
- (void)releasePlayer;

// Playback control
- (BOOL)openFile:(NSString *)filePath;
- (BOOL)openURL:(NSURL *)url;
- (void)play;
- (void)pause;
- (void)stop;
- (void)seekTo:(NSTimeInterval)time;

// Rendering
- (void)setRenderView:(NSView *)view;

@end

NS_ASSUME_NONNULL_END
