# macOS 播放器 Demo 实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use horspowers:subagent-driven-development to implement this plan task-by-task.

**日期**: 2026-04-20

## 目标

创建一个功能完善的 macOS 播放器 Demo，支持本地文件播放、软硬编解码切换、基础播放控制，用于 SDK 的集成测试和功能验证。

## 架构方案

使用 macOS AppKit 构建原生 macOS 应用程序，通过 Objective-C++ 桥接层调用 C++ SDK。界面使用 XIB/NIB 或纯代码布局，核心播放功能通过 HorsAVSDK 的 Player API 实现。

## 技术栈

- macOS AppKit / Cocoa
- Objective-C++ (mm 文件)
- HorsAVSDK C++ API
- Metal 视频渲染

---

## Task 0: 创建 macOS Demo 项目结构

**Files:**
- Create: `examples/macos_player/CMakeLists.txt`
- Create: `examples/macos_player/Info.plist`
- Modify: `examples/CMakeLists.txt`

**Step 1: 创建 CMake 配置**

```cmake
# examples/macos_player/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-arc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc -std=c++14")

# macOS Player Demo
set(MACOS_PLAYER_SOURCES
    main.m
    AppDelegate.mm
    ViewController.mm
    PlayerView.mm
    PlayerWrapper.mm
    main.mm
)

set(MACOS_PLAYER_HEADERS
    AppDelegate.h
    ViewController.h
    PlayerView.h
    PlayerWrapper.h
)

# Create macOS App Bundle
add_executable(macos_player MACOSX_BUNDLE
    ${MACOS_PLAYER_SOURCES}
    ${MACOS_PLAYER_HEADERS}
)

set_target_properties(macos_player PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    MACOSX_BUNDLE_BUNDLE_NAME "HorsAVPlayer"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.hors.avplayer"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
)

target_link_libraries(macos_player PRIVATE
    avsdk_core
    avsdk_platform
    "-framework Cocoa"
    "-framework Metal"
    "-framework MetalKit"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework AVFoundation"
)

target_include_directories(macos_player PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
```

**Step 2: 创建 Info.plist**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>${EXECUTABLE_NAME}</string>
    <key>CFBundleIdentifier</key>
    <string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${PRODUCT_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.14</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
```

**Step 3: 更新 examples/CMakeLists.txt**

```cmake
# Add macOS player demo
if(APPLE AND PLATFORM STREQUAL "macOS")
    add_subdirectory(macos_player)
endif()
```

**Step 4: 运行构建测试**

Run: `cd build/macos && cmake ../.. && make macos_player 2>&1 | tail -20`
Expected: Configured successfully (will fail to compile without source files)

**Step 5: Commit**

```bash
git add examples/macos_player/CMakeLists.txt examples/macos_player/Info.plist examples/CMakeLists.txt
git commit -m "build(macos): add macOS player demo project structure"
```

---

## Task 1: 创建基础 App 结构

**Files:**
- Create: `examples/macos_player/main.m`
- Create: `examples/macos_player/AppDelegate.h`
- Create: `examples/macos_player/AppDelegate.mm`

**Step 1: 创建 main.m**

```objc
// examples/macos_player/main.m
#import <Cocoa/Cocoa.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        return NSApplicationMain(argc, argv);
    }
}
```

**Step 2: 创建 AppDelegate.h**

```objc
// examples/macos_player/AppDelegate.h
#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property (strong) NSWindow *window;
@property (strong) NSViewController *viewController;

@end
```

**Step 3: 创建 AppDelegate.mm**

```objc
// examples/macos_player/AppDelegate.mm
#import "AppDelegate.h"
#import "ViewController.h"

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Create window
    NSRect frame = NSMakeRect(100, 100, 1280, 720);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:(NSWindowStyleMaskTitled |
                                                         NSWindowStyleMaskClosable |
                                                         NSWindowStyleMaskMiniaturizable |
                                                         NSWindowStyleMaskResizable)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    
    self.window.title = @"HorsAVPlayer - macOS Demo";
    self.window.minSize = NSMakeSize(640, 480);
    
    // Create view controller
    self.viewController = [[ViewController alloc] init];
    self.window.contentViewController = self.viewController;
    
    [self.window makeKeyAndOrderFront:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end
```

**Step 4: 编译测试**

Run: `cd build/macos && make macos_player 2>&1 | head -30`
Expected: Compiles successfully

**Step 5: Commit**

```bash
git add examples/macos_player/main.m examples/macos_player/AppDelegate.h examples/macos_player/AppDelegate.mm
git commit -m "feat(macos-demo): add basic App structure"
```

---

## Task 2: 创建 PlayerView (Metal 渲染视图)

**Files:**
- Create: `examples/macos_player/PlayerView.h`
- Create: `examples/macos_player/PlayerView.mm`

**Step 1: 创建 PlayerView.h**

```objc
// examples/macos_player/PlayerView.h
#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

@protocol PlayerViewDelegate <NSObject>
- (void)playerView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size;
- (void)renderInView:(nonnull MTKView *)view;
@end

@interface PlayerView : NSView

@property (nonatomic, weak, nullable) id<PlayerViewDelegate> delegate;
@property (nonatomic, readonly, nonnull) MTKView *mtkView;

- (void)setupMetal;
- (CGSize)drawableSize;

@end
```

**Step 2: 创建 PlayerView.mm**

```objc
// examples/macos_player/PlayerView.mm
#import "PlayerView.h"
#import <Metal/Metal.h>

@interface PlayerView () <MTKViewDelegate>
@end

@implementation PlayerView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setupMetal];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    self = [super initWithCoder:coder];
    if (self) {
        [self setupMetal];
    }
    return self;
}

- (void)setupMetal {
    // Create MTKView
    _mtkView = [[MTKView alloc] initWithFrame:self.bounds];
    _mtkView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _mtkView.device = MTLCreateSystemDefaultDevice();
    _mtkView.delegate = self;
    _mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    _mtkView.depthStencilPixelFormat = MTLPixelFormatInvalid;
    _mtkView.enableSetNeedsDisplay = NO;
    _mtkView.paused = NO;
    
    [self addSubview:_mtkView];
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
    [super resizeSubviewsWithOldSize:oldSize];
    _mtkView.frame = self.bounds;
}

- (CGSize)drawableSize {
    return _mtkView.drawableSize;
}

#pragma mark - MTKViewDelegate

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    if ([self.delegate respondsToSelector:@selector(playerView:drawableSizeWillChange:)]) {
        [self.delegate playerView:view drawableSizeWillChange:size];
    }
}

- (void)drawInView:(MTKView *)view {
    if ([self.delegate respondsToSelector:@selector(renderInView:)]) {
        [self.delegate renderInView:view];
    }
}

@end
```

**Step 3: 编译测试**

Run: `cd build/macos && make macos_player 2>&1 | head -30`
Expected: Compiles successfully

**Step 4: Commit**

```bash
git add examples/macos_player/PlayerView.h examples/macos_player/PlayerView.mm
git commit -m "feat(macos-demo): add Metal-backed PlayerView"
```

---

## Task 3: 创建 PlayerWrapper (SDK 桥接层)

**Files:**
- Create: `examples/macos_player/PlayerWrapper.h`
- Create: `examples/macos_player/PlayerWrapper.mm`

**Step 1: 创建 PlayerWrapper.h**

```objc
// examples/macos_player/PlayerWrapper.h
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

typedef NS_ENUM(NSInteger, PlayerState) {
    PlayerStateIdle = 0,
    PlayerStateReady,
    PlayerStateBuffering,
    PlayerStatePlaying,
    PlayerStatePaused,
    PlayerStateError
};

typedef NS_ENUM(NSInteger, DecoderMode) {
    DecoderModeAuto = 0,
    DecoderModeSoftware,
    DecoderModeHardware,
    DecoderModeHardwareFirst
};

@class PlayerWrapper;

@protocol PlayerWrapperDelegate <NSObject>
@optional
- (void)player:(PlayerWrapper *)player stateChanged:(PlayerState)state;
- (void)player:(PlayerWrapper *)player didUpdateProgress:(NSTimeInterval)currentTime duration:(NSTimeInterval)duration;
- (void)player:(PlayerWrapper *)player didEncounterError:(NSError *)error;
@end

@interface PlayerWrapper : NSObject

@property (nonatomic, weak) id<PlayerWrapperDelegate> delegate;
@property (nonatomic, readonly) PlayerState state;
@property (nonatomic, readonly) NSTimeInterval currentTime;
@property (nonatomic, readonly) NSTimeInterval duration;
@property (nonatomic, readonly) CGSize videoSize;

// Configuration
@property (nonatomic) DecoderMode decoderMode;
@property (nonatomic) BOOL enableHardwareDecoder;

// Lifecycle
- (BOOL)initializePlayer;
- (void)releasePlayer;

// Playback control
- (BOOL)openFile:(NSString *)path;
- (BOOL)play;
- (BOOL)pause;
- (BOOL)stop;
- (BOOL)seekTo:(NSTimeInterval)time;

// Rendering
- (void)setRenderView:(NSView *)view;

@end
```

**Step 2: 创建 PlayerWrapper.mm**

```objc
// examples/macos_player/PlayerWrapper.mm
#import "PlayerWrapper.h"
#include "avsdk/player.h"
#include "avsdk/player_config.h"
#include "avsdk/error.h"
#include "avsdk/metal_renderer.h"

using namespace AVSDK;

@interface PlayerWrapper () {
    std::shared_ptr<IPlayer> _player;
    std::shared_ptr<IVideoRenderer> _renderer;
}
@end

@implementation PlayerWrapper

- (BOOL)initializePlayer {
    if (_player) {
        return YES;
    }
    
    _player = PlayerFactory::CreatePlayer();
    if (!_player) {
        return NO;
    }
    
    // Configure player
    PlayerConfig config;
    config.decodeMode = [self avsdkDecodeMode];
    config.enableHardwareDecoder = _enableHardwareDecoder;
    config.bufferStrategy = BufferStrategy::Smooth;
    
    ErrorCode result = _player->Initialize(config);
    if (result != ErrorCode::OK) {
        NSLog(@"Failed to initialize player: %d", static_cast<int>(result));
        return NO;
    }
    
    // Set up callbacks
    __weak typeof(self) weakSelf = self;
    PlayerCallbacks callbacks;
    callbacks.onStateChanged = [weakSelf](PlayerState oldState, PlayerState newState) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf handleStateChange:newState];
        });
    };
    callbacks.onProgressUpdate = [weakSelf](Timestamp currentMs, Timestamp durationMs) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf handleProgressUpdate:currentMs duration:durationMs];
        });
    };
    callbacks.onError = [weakSelf](const ErrorInfo& error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf handleError:error];
        });
    };
    
    _player->SetCallbacks(callbacks);
    
    return YES;
}

- (void)releasePlayer {
    if (_player) {
        _player->Release();
        _player.reset();
    }
    _renderer.reset();
}

- (BOOL)openFile:(NSString *)path {
    if (!_player) {
        return NO;
    }
    
    ErrorCode result = _player->SetDataSource([path UTF8String]);
    if (result != ErrorCode::OK) {
        NSLog(@"Failed to open file: %d", static_cast<int>(result));
        return NO;
    }
    
    result = _player->PrepareSync();
    if (result != ErrorCode::OK) {
        NSLog(@"Failed to prepare: %d", static_cast<int>(result));
        return NO;
    }
    
    // Get video size
    auto info = _player->GetMediaInfo();
    _videoSize = CGSizeMake(info.videoResolution.width, info.videoResolution.height);
    _duration = info.duration / 1000.0;
    
    return YES;
}

- (BOOL)play {
    if (!_player) return NO;
    ErrorCode result = _player->Start();
    return result == ErrorCode::OK;
}

- (BOOL)pause {
    if (!_player) return NO;
    ErrorCode result = _player->Pause();
    return result == ErrorCode::OK;
}

- (BOOL)stop {
    if (!_player) return NO;
    ErrorCode result = _player->Stop();
    return result == ErrorCode::OK;
}

- (BOOL)seekTo:(NSTimeInterval)time {
    if (!_player) return NO;
    ErrorCode result = _player->SeekToSync(static_cast<Timestamp>(time * 1000));
    return result == ErrorCode::OK;
}

- (void)setRenderView:(NSView *)view {
    if (!_player || !view) return;
    
    // Create Metal renderer
    _renderer = CreateMetalRenderer((__bridge void*)view);
    if (_renderer) {
        _player->SetVideoRenderer(_renderer);
    }
}

- (PlayerState)state {
    if (!_player) return PlayerStateIdle;
    return [self nsStateFromAVSDK:_player->GetState()];
}

- (NSTimeInterval)currentTime {
    if (!_player) return 0;
    return _player->GetCurrentPosition() / 1000.0;
}

#pragma mark - Private Methods

- (DecodeMode)avsdkDecodeMode {
    switch (_decoderMode) {
        case DecoderModeSoftware: return DecodeMode::Software;
        case DecoderModeHardware: return DecodeMode::Hardware;
        case DecoderModeHardwareFirst: return DecodeMode::HardwareFirst;
        default: return DecodeMode::Auto;
    }
}

- (PlayerState)nsStateFromAVSDK:(AVSDK::PlayerState)state {
    switch (state) {
        case AVSDK::PlayerState::Ready: return PlayerStateReady;
        case AVSDK::PlayerState::Buffering: return PlayerStateBuffering;
        case AVSDK::PlayerState::Playing: return PlayerStatePlaying;
        case AVSDK::PlayerState::Paused: return PlayerStatePaused;
        case AVSDK::PlayerState::Error: return PlayerStateError;
        default: return PlayerStateIdle;
    }
}

- (void)handleStateChange:(AVSDK::PlayerState)state {
    if ([self.delegate respondsToSelector:@selector(player:stateChanged:)]) {
        [self.delegate player:self stateChanged:[self nsStateFromAVSDK:state]];
    }
}

- (void)handleProgressUpdate:(Timestamp)currentMs duration:(Timestamp)durationMs {
    if ([self.delegate respondsToSelector:@selector(player:didUpdateProgress:duration:)]) {
        [self.delegate player:self 
              didUpdateProgress:currentMs / 1000.0 
                       duration:durationMs / 1000.0];
    }
}

- (void)handleError:(const ErrorInfo&)error {
    if ([self.delegate respondsToSelector:@selector(player:didEncounterError:)]) {
        NSError *nsError = [NSError errorWithDomain:@"AVSDK" 
                                               code:static_cast<int>(error.code) 
                                           userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithUTF8String:error.message.c_str()]}];
        [self.delegate player:self didEncounterError:nsError];
    }
}

@end
```

**Step 3: 编译测试**

Run: `cd build/macos && make macos_player 2>&1 | head -50`
Expected: Compiles successfully

**Step 4: Commit**

```bash
git add examples/macos_player/PlayerWrapper.h examples/macos_player/PlayerWrapper.mm
git commit -m "feat(macos-demo): add PlayerWrapper SDK bridge"
```

---

## Task 4: 创建主界面 ViewController

**Files:**
- Create: `examples/macos_player/ViewController.h`
- Create: `examples/macos_player/ViewController.mm`

**Step 1: 创建 ViewController.h**

```objc
// examples/macos_player/ViewController.h
#import <Cocoa/Cocoa.h>
#import "PlayerView.h"
#import "PlayerWrapper.h"

@interface ViewController : NSViewController <PlayerViewDelegate, PlayerWrapperDelegate>

@property (strong) PlayerView *playerView;
@property (strong) PlayerWrapper *player;

// UI Controls
@property (strong) NSToolbar *toolbar;
@property (strong) NSSlider *progressSlider;
@property (strong) NSTextField *timeLabel;
@property (strong) NSPopUpButton *decoderModePopup;
@property (strong) NSButton *playPauseButton;
@property (strong) NSButton *stopButton;
@property (strong) NSButton *openButton;

@end
```

**Step 2: 创建 ViewController.mm**

```objc
// examples/macos_player/ViewController.mm
#import "ViewController.h"

@interface ViewController ()
@property (strong) NSTimer *progressTimer;
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.view.wantsLayer = YES;
    self.view.layer.backgroundColor = [NSColor blackColor].CGColor;
    
    [self setupUI];
    [self setupPlayer];
}

- (void)setupUI {
    // Player View
    self.playerView = [[PlayerView alloc] initWithFrame:self.view.bounds];
    self.playerView.delegate = self;
    self.playerView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self.view addSubview:self.playerView];
    
    // Controls overlay
    NSView *controlsView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, self.view.bounds.size.width, 80)];
    controlsView.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;
    controlsView.wantsLayer = YES;
    controlsView.layer.backgroundColor = [NSColor colorWithWhite:0 alpha:0.7].CGColor;
    [self.view addSubview:controlsView];
    
    // Open button
    self.openButton = [NSButton buttonWithTitle:@"Open" target:self action:@selector(openFile:)];
    self.openButton.frame = NSMakeRect(20, 40, 80, 30);
    [controlsView addSubview:self.openButton];
    
    // Play/Pause button
    self.playPauseButton = [NSButton buttonWithTitle:@"Play" target:self action:@selector(togglePlayPause:)];
    self.playPauseButton.frame = NSMakeRect(110, 40, 80, 30);
    self.playPauseButton.enabled = NO;
    [controlsView addSubview:self.playPauseButton];
    
    // Stop button
    self.stopButton = [NSButton buttonWithTitle:@"Stop" target:self action:@selector(stop:)];
    self.stopButton.frame = NSMakeRect(200, 40, 80, 30);
    self.stopButton.enabled = NO;
    [controlsView addSubview:self.stopButton];
    
    // Decoder mode popup
    NSTextField *decoderLabel = [NSTextField labelWithString:@"Decoder:"];
    decoderLabel.frame = NSMakeRect(300, 45, 60, 20);
    [controlsView addSubview:decoderLabel];
    
    self.decoderModePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(360, 40, 150, 30) pullsDown:NO];
    [self.decoderModePopup addItemWithTitle:@"Auto"];
    [self.decoderModePopup addItemWithTitle:@"Software"];
    [self.decoderModePopup addItemWithTitle:@"Hardware"];
    [self.decoderModePopup addItemWithTitle:@"Hardware First"];
    [self.decoderModePopup setTarget:self];
    [self.decoderModePopup setAction:@selector(decoderModeChanged:)];
    [controlsView addSubview:self.decoderModePopup];
    
    // Progress slider
    self.progressSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 10, self.view.bounds.size.width - 200, 20)];
    self.progressSlider.autoresizingMask = NSViewWidthSizable;
    self.progressSlider.minValue = 0;
    self.progressSlider.maxValue = 1;
    self.progressSlider.enabled = NO;
    [self.progressSlider setTarget:self];
    [self.progressSlider setAction:@selector(sliderChanged:)];
    [controlsView addSubview:self.progressSlider];
    
    // Time label
    self.timeLabel = [NSTextField labelWithString:@"00:00 / 00:00"];
    self.timeLabel.frame = NSMakeRect(self.view.bounds.size.width - 170, 10, 150, 20);
    self.timeLabel.autoresizingMask = NSViewMinXMargin;
    self.timeLabel.alignment = NSTextAlignmentRight;
    self.timeLabel.textColor = [NSColor whiteColor];
    [controlsView addSubview:self.timeLabel];
}

- (void)setupPlayer {
    self.player = [[PlayerWrapper alloc] init];
    self.player.delegate = self;
    
    if (![self.player initializePlayer]) {
        [self showError:@"Failed to initialize player"];
        return;
    }
    
    [self.player setRenderView:self.playerView.mtkView];
}

#pragma mark - Actions

- (void)openFile:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.allowedFileTypes = @[@"mp4", @"mov", @"mkv", @"avi", @"flv", @"m4v"];
    panel.allowsMultipleSelection = NO;
    
    [panel beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSURL *url = panel.URL;
            [self loadFile:url.path];
        }
    }];
}

- (void)loadFile:(NSString *)path {
    [self.player stop];
    
    if ([self.player openFile:path]) {
        self.playPauseButton.title = @"Play";
        self.playPauseButton.enabled = YES;
        self.stopButton.enabled = YES;
        self.progressSlider.enabled = YES;
        
        [self updateTimeLabel];
        
        // Auto play
        [self.player play];
        self.playPauseButton.title = @"Pause";
        
        // Start progress timer
        [self startProgressTimer];
    } else {
        [self showError:@"Failed to open file"];
    }
}

- (void)togglePlayPause:(id)sender {
    if (self.player.state == PlayerStatePlaying) {
        [self.player pause];
        self.playPauseButton.title = @"Play";
    } else {
        [self.player play];
        self.playPauseButton.title = @"Pause";
    }
}

- (void)stop:(id)sender {
    [self.player stop];
    self.playPauseButton.title = @"Play";
    self.progressSlider.doubleValue = 0;
    [self updateTimeLabel];
    [self stopProgressTimer];
}

- (void)sliderChanged:(id)sender {
    NSTimeInterval time = self.progressSlider.doubleValue * self.player.duration;
    [self.player seekTo:time];
}

- (void)decoderModeChanged:(id)sender {
    self.player.decoderMode = (DecoderMode)self.decoderModePopup.indexOfSelectedItem;
    // Reinitialize player with new settings
    [self.player releasePlayer];
    [self.player initializePlayer];
    [self.player setRenderView:self.playerView.mtkView];
}

#pragma mark - Progress Updates

- (void)startProgressTimer {
    [self stopProgressTimer];
    self.progressTimer = [NSTimer scheduledTimerWithTimeInterval:0.5
                                                          target:self
                                                        selector:@selector(updateProgress)
                                                        userInfo:nil
                                                         repeats:YES];
}

- (void)stopProgressTimer {
    [self.progressTimer invalidate];
    self.progressTimer = nil;
}

- (void)updateProgress {
    if (self.player.duration > 0) {
        double progress = self.player.currentTime / self.player.duration;
        self.progressSlider.doubleValue = progress;
        [self updateTimeLabel];
    }
}

- (void)updateTimeLabel {
    NSString *current = [self formatTime:self.player.currentTime];
    NSString *duration = [self formatTime:self.player.duration];
    self.timeLabel.stringValue = [NSString stringWithFormat:@"%@ / %@", current, duration];
}

- (NSString *)formatTime:(NSTimeInterval)time {
    int minutes = (int)time / 60;
    int seconds = (int)time % 60;
    return [NSString stringWithFormat:@"%02d:%02d", minutes, seconds];
}

- (void)showError:(NSString *)message {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Error";
    alert.informativeText = message;
    alert.alertStyle = NSAlertStyleCritical;
    [alert runModal];
}

#pragma mark - PlayerWrapperDelegate

- (void)player:(PlayerWrapper *)player stateChanged:(PlayerState)state {
    switch (state) {
        case PlayerStatePlaying:
            self.playPauseButton.title = @"Pause";
            break;
        case PlayerStatePaused:
            self.playPauseButton.title = @"Play";
            break;
        case PlayerStateError:
            [self showError:@"Playback error"];
            break;
        default:
            break;
    }
}

- (void)player:(PlayerWrapper *)player didUpdateProgress:(NSTimeInterval)currentTime duration:(NSTimeInterval)duration {
    // Handled by timer
}

- (void)player:(PlayerWrapper *)player didEncounterError:(NSError *)error {
    [self showError:error.localizedDescription];
}

#pragma mark - PlayerViewDelegate

- (void)playerView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    // Handle resize
}

- (void)renderInView:(MTKView *)view {
    // Rendering handled by SDK
}

- (void)viewWillDisappear {
    [super viewWillDisappear];
    [self.player releasePlayer];
    [self stopProgressTimer];
}

@end
```

**Step 3: 编译测试**

Run: `cd build/macos && make macos_player 2>&1 | head -50`
Expected: Compiles successfully

**Step 4: Commit**

```bash
git add examples/macos_player/ViewController.h examples/macos_player/ViewController.mm
git commit -m "feat(macos-demo): add ViewController with full UI"
```

---

## Task 5: 修复构建并运行 Demo

**Step 1: 修复可能的编译问题**

检查并修复：
- Metal 渲染器头文件路径
- Objective-C++ 编译标志
- 框架链接

**Step 2: 完整构建**

Run:
```bash
cd build/macos
make clean
make macos_player -j$(sysctl -n hw.ncpu) 2>&1
```

**Step 3: 运行 Demo**

Run:
```bash
open examples/macos_player/macos_player.app
# 或者
./examples/macos_player/macos_player.app/Contents/MacOS/macos_player /path/to/test.mp4
```

**Step 4: 功能验证**

测试以下功能：
- [ ] 打开本地视频文件
- [ ] 播放/暂停控制
- [ ] 停止并重置
- [ ] 拖动进度条跳转
- [ ] 切换软硬编解码模式
- [ ] 窗口缩放适应

**Step 5: Commit**

```bash
git commit -m "feat(macos-demo): complete macOS player demo

- Support local file playback
- Support hardware/software decoder switching
- Full playback controls (play/pause/stop/seek)
- Progress display and time tracking"
```

---

## 验收标准

- [ ] Demo 应用可正常编译为 macOS App Bundle
- [ ] 支持打开本地视频文件 (MP4/MOV/MKV/AVI)
- [ ] 支持播放/暂停/停止控制
- [ ] 支持进度条拖动跳转
- [ ] 支持软硬编解码模式切换 (Auto/Software/Hardware/HardwareFirst)
- [ ] Metal 视频渲染正常
- [ ] 窗口可缩放，视频自适应
- [ ] 播放状态实时显示

---

## 使用说明

### 运行 Demo

```bash
# 构建
cd build/macos
make macos_player

# 运行
open examples/macos_player/macos_player.app
```

### 功能说明

1. **Open 按钮** - 打开本地视频文件
2. **Play/Pause 按钮** - 播放或暂停
3. **Stop 按钮** - 停止播放并重置进度
4. **Decoder 下拉菜单** - 切换编解码模式
5. **进度条** - 显示和拖动跳转播放进度
6. **时间显示** - 当前时间 / 总时长

---

*计划完成，准备执行*
