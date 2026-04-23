# HorsAVSDK Objective-C/Swift 接口规范

**版本**: 1.0  
**最后更新**: 2024-04-22  
**维护者**: HorsAVSDK Team

---

## 目录

1. [概述](#1-概述)
2. [设计原则](#2-设计原则)
3. [模块架构](#3-模块架构)
4. [核心类型映射](#4-核心类型映射)
5. [接口规范](#5-接口规范)
6. [模块兼容性](#6-模块兼容性)
7. [版本管理策略](#7-版本管理策略)
8. [示例代码](#8-示例代码)

---

## 1. 概述

本文档定义 HorsAVSDK 的 Objective-C/Swift 接口规范，用于将 C++ 核心 SDK 封装为 iOS/macOS 原生 API。

### 1.1 目标

- 提供 idiomatic Objective-C API
- 完整 Swift 互操作性（包括 async/await）
- Framework 模块化支持
- 与 C++ SDK 版本同步

### 1.2 目录结构

```
bindings/objc/
├── HorsAVSDK.h                      # Umbrella header
├── HorsAVSDK.modulemap              # 模块定义
├── HorsAVSDK-umbrella.h             # 框架头文件
├── HorsAVPlayer.h                   # 播放器主类
├── HorsAVPlayer.mm                  # 播放器实现
├── HorsAVPlayerConfig.h             # 配置类
├── HorsAVTypes.h                    # 公共类型
├── HorsAVError.h                    # 错误处理
├── HorsAVRenderer.h                 # 渲染相关
├── HorsAVDataBypass.h               # 数据旁路
├── HorsAVLogger.h                   # 日志系统
├── Internal/                        # 内部实现
│   ├── HorsAVPlayerInternal.h
│   ├── HorsAVCallbackBridge.h
│   └── HorsAVUtils.hpp
└── Swift/                           # Swift 扩展
    └── HorsAVPlayer+Swift.swift
```

---

## 2. 设计原则

### 2.1 命名规范

| C++ 命名 | Objective-C 命名 | 说明 |
|---------|-----------------|------|
| `IPlayer` | `HorsAVPlayer` | 去掉 I 前缀，加 HorsAV 前缀 |
| `Initialize()` | `- initWithConfig:` | Objective-C 风格构造 |
| `SetRenderView()` | `- setRenderView:` | property setter 风格 |
| `GetState()` | `state` (property) | getter 转 property |
| `kStopped` | `HorsAVPlayerStateStopped` | 枚举加前缀 |
| `ErrorCode` | `HorsAVErrorCode` | 错误码加前缀 |

### 2.2 Swift 互操作性标记

```objc
// 类名映射
NS_SWIFT_NAME(Player)  // Swift 中显示为 Player

// 方法名映射  
NS_SWIFT_NAME(seek(to:))  // Swift: player.seek(to: 10.0)

// 枚举映射
NS_SWIFT_NAME(PlayerState)
typedef NS_ENUM(NSInteger, HorsAVPlayerState) { ... };

// Nullability
nullable / nonnull
NS_ASSUME_NONNULL_BEGIN/END

// 错误处理
NS_ERROR_ENUM  // Swift throws 支持
```

### 2.3 内存管理

- 所有公开类使用 ARC (`-fobjc-arc`)
- C++ 对象使用 `std::shared_ptr` 包装在内部
- 使用 `__attribute__((objc_precise_lifetime))` 确保生命周期正确

---

## 3. 模块架构

### 3.1 模块定义 (modulemap)

```modulemap
// HorsAVSDK.modulemap
module HorsAVSDK {
    umbrella header "HorsAVSDK.h"
    
    export *
    
    // 显式导出 Metal 扩展
    explicit module Metal {
        header "HorsAVMetalRenderer.h"
    }
}
```

### 3.2 Swift Package Manager 支持

```swift
// Package.swift
.target(
    name: "HorsAVSDK",
    dependencies: [],
    path: "bindings/objc",
    publicHeadersPath: "include",
    cxxSettings: [
        .headerSearchPath("../../include"),
        .headerSearchPath("../../src"),
    ]
)
```

---

## 4. 核心类型映射

### 4.1 基础类型

| C++ 类型 | Objective-C 类型 | Swift 映射 | 说明 |
|---------|-----------------|------------|------|
| `int64_t` | `int64_t` | `Int64` | 直接使用 |
| `int32_t` | `int32_t` | `Int32` | 直接使用 |
| `size_t` | `NSUInteger` | `UInt` | 无符号整数 |
| `float` | `float` | `Float` | 直接使用 |
| `double` | `double` | `Double` | 直接使用 |
| `bool` | `BOOL` | `Bool` | Objective-C 布尔 |
| `std::string` | `NSString *` | `String` | 自动转换 |
| `void*` | `void *` | `UnsafeMutableRawPointer?` | 原生指针 |

### 4.2 枚举映射

```objc
// C++
enum class PlayerState { kIdle, kStopped, kPlaying, kPaused, kError };

// Objective-C
NS_SWIFT_NAME(PlayerState)
typedef NS_ENUM(NSInteger, HorsAVPlayerState) {
    HorsAVPlayerStateIdle     = 0,
    HorsAVPlayerStateStopped  = 1,
    HorsAVPlayerStatePlaying  = 2,
    HorsAVPlayerStatePaused   = 3,
    HorsAVPlayerStateError    = 4
} NS_SWIFT_NAME(PlayerState);
```

### 4.3 错误码映射

```objc
// Objective-C Error Domain
NS_SWIFT_NAME(HorsAVErrorDomain)
extern NSErrorDomain const HorsAVErrorDomain;

// 错误码枚举
NS_SWIFT_NAME(ErrorCode)
typedef NS_ERROR_ENUM(HorsAVErrorDomain, HorsAVErrorCode) {
    HorsAVErrorCodeOK                      = 0,
    HorsAVErrorCodeUnknown                 = 1,
    HorsAVErrorCodeInvalidParameter        = 2,
    HorsAVErrorCodeOutOfMemory             = 4,
    HorsAVErrorCodeNotInitialized          = 6,
    HorsAVErrorCodePlayerOpenFailed        = 103,
    HorsAVErrorCodePlayerSeekFailed        = 104,
    HorsAVErrorCodeCodecNotFound           = 200,
    HorsAVErrorCodeNetworkConnectFailed    = 300,
    HorsAVErrorCodeFileNotFound            = 400,
    HorsAVErrorCodeHardwareNotAvailable    = 500,
};
```

### 4.4 结构体映射

```objc
// C++: struct MediaInfo { ... };
// Objective-C: 使用不可变对象
NS_SWIFT_NAME(MediaInfo)
@interface HorsAVMediaInfo : NSObject <NSCopying>

@property (nonatomic, readonly, copy) NSString *url;
@property (nonatomic, readonly, copy) NSString *format;
@property (nonatomic, readonly) NSTimeInterval duration;
@property (nonatomic, readonly) CGSize videoSize;
@property (nonatomic, readonly) NSInteger audioSampleRate;
@property (nonatomic, readonly) NSInteger audioChannels;
@property (nonatomic, readonly) BOOL hasVideo;
@property (nonatomic, readonly) BOOL hasAudio;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end
```

### 4.5 配置结构体

```objc
NS_SWIFT_NAME(PlayerConfiguration)
@interface HorsAVPlayerConfiguration : NSObject <NSCopying, NSMutableCopying>

@property (nonatomic, readwrite) BOOL enableHardwareDecoder;
@property (nonatomic, readwrite) NSTimeInterval bufferTime;
@property (nonatomic, readwrite, copy) NSString *logLevel;

// 便捷构造器
+ (instancetype)defaultConfiguration NS_SWIFT_NAME(default());

@end
```

### 4.6 回调接口映射

```objc
// C++: class IPlayerCallback { ... };
// Objective-C: Protocol
NS_SWIFT_NAME(PlayerDelegate)
@protocol HorsAVPlayerDelegate <NSObject>

@optional
- (void)player:(HorsAVPlayer *)player didPrepareMedia:(HorsAVMediaInfo *)info;
- (void)player:(HorsAVPlayer *)player didChangeState:(HorsAVPlayerState)oldState 
                                            toState:(HorsAVPlayerState)newState;
- (void)player:(HorsAVPlayer *)player didEncounterError:(NSError *)error;
- (void)playerDidCompletePlayback:(HorsAVPlayer *)player;
- (void)player:(HorsAVPlayer *)player didUpdateProgress:(NSTimeInterval)currentTime 
                                             duration:(NSTimeInterval)duration;
- (void)player:(HorsAVPlayer *)player didChangeBuffering:(BOOL)isBuffering 
                                               percent:(NSInteger)percent;
- (void)player:(HorsAVPlayer *)player didCompleteSeekTo:(NSTimeInterval)position;

@end
```

---

## 5. 接口规范

### 5.1 播放器主类 (HorsAVPlayer)

```objc
// HorsAVPlayer.h
#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>  // For MTKView
#import "HorsAVTypes.h"

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(Player)
@interface HorsAVPlayer : NSObject

#pragma mark - Initialization

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

/// 使用配置初始化播放器
/// @param configuration 播放器配置，nil 时使用默认配置
/// @param error 错误输出
/// @return 初始化后的播放器实例，失败时返回 nil
- (nullable instancetype)initWithConfiguration:(nullable HorsAVPlayerConfiguration *)configuration
                                         error:(NSError **)error NS_SWIFT_NAME(init(configuration:)) NS_DESIGNATED_INITIALIZER;

#pragma mark - Properties

/// 播放器委托
@property (nonatomic, weak, nullable) id<HorsAVPlayerDelegate> delegate;

/// 当前播放状态
@property (nonatomic, readonly) HorsAVPlayerState state;

/// 当前播放位置（秒）
@property (nonatomic, readonly) NSTimeInterval currentTime;

/// 媒体总时长（秒），未知时为 0
@property (nonatomic, readonly) NSTimeInterval duration;

/// 视频尺寸
@property (nonatomic, readonly) CGSize videoSize;

/// 是否正在缓冲
@property (nonatomic, readonly, getter=isBuffering) BOOL buffering;

/// 缓冲百分比 (0-100)
@property (nonatomic, readonly) NSInteger bufferingPercent;

#pragma mark - Playback Control

/// 打开媒体文件或流
/// @param url 媒体 URL（本地文件或网络地址）
/// @param error 错误输出
/// @return 成功返回 YES，失败时返回 NO 并设置 error
- (BOOL)openURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(open(_:));

/// 开始播放
- (void)play NS_SWIFT_NAME(play());

/// 暂停播放
- (void)pause NS_SWIFT_NAME(pause());

/// 停止播放
- (void)stop NS_SWIFT_NAME(stop());

/// 定位到指定位置
/// @param time 目标时间（秒）
/// @param completionHandler 完成回调
- (void)seekToTime:(NSTimeInterval)time completionHandler: (void (^_Nullable)(BOOL success, NSTimeInterval actualTime, NSError *_Nullable error))completionHandlerNS_SWIFT_NAME(seek(to:completionHandler:);

///关闭当前媒体
- (void)close NS_SWIFT_NAME(close());

#pragma mark - Rendering

/// 设置渲染视图（Metal）
/// @param view MTKView 实例
- (void)setRenderView:(MTKView *)view NS_SWIFT_NAME(setRenderView(_:));

@end

NS_ASSUME_NONNULL_END
```

### 5.2 配置类 (HorsAVPlayerConfiguration)

```objc
// HorsAVPlayerConfig.h
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(DecoderMode)
typedef NS_ENUM(NSInteger, HorsAVDecoderMode) {
    HorsAVDecoderModeAuto,          // 自动选择
    HorsAVDecoderModeSoftware,      // 强制软件解码
    HorsAVDecoderModeHardware,    // 强制硬件解码
    HorsAVDecoderModeHardwareFirst // 优先硬件，失败时回退软件
} NS_SWIFT_NAME(DecoderMode);

NS_SWIFT_NAME(PlayerConfiguration)
@interface HorsAVPlayerConfiguration : NSObject <NSCopying, NSMutableCopying>

/// 解码器模式，默认为 Auto
@property (nonatomic, readwrite) HorsAVDecoderMode decoderMode;

/// 启用硬件解码，默认为 YES
@property (nonatomic, readwrite) BOOL enableHardwareDecoder;

/// 缓冲时间（秒），默认为 1.0
@property (nonatomic, readwrite) NSTimeInterval bufferTime;

/// 日志级别，可选值: "trace", "debug", "info", "warning", "error", "off"
@property (nonatomic, readwrite, copy) NSString *logLevel;

/// 默认配置
+ (instancetype)defaultConfiguration NS_SWIFT_NAME(default());

@end

NS_ASSUME_NONNULL_END
```

### 5.3 数据旁路接口 (HorsAVDataBypass)

```objc
// HorsAVDataBypass.h
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// 数据旁路阶段
NS_SWIFT_NAME(DataBypassStage)
typedef NS_ENUM(NSInteger, HorsAVDataBypassStage) {
    HorsAVDataBypassStageDemuxed,    // 解封装后
    HorsAVDataBypassStageDecoded,    // 解码后
    HorsAVDataBypassStageProcessed,  // 处理后
    HorsAVDataBypassStageRendered    // 渲染前/后
} NS_SWIFT_NAME(DataBypassStage);

// 数据类型
NS_SWIFT_NAME(DataType)
typedef NS_ENUM(NSInteger, HorsAVDataType) {
    HorsAVDataTypeVideo,
    HorsAVDataTypeAudio
} NS_SWIFT_NAME(DataType);

// 视频帧数据
NS_SWIFT_NAME(VideoFrame)
@interface HorsAVVideoFrame : NSObject
@property (nonatomic, readonly) CGSize size;
@property (nonatomic, readonly) int64_t timestamp; // 毫秒
@property (nonatomic, readonly) BOOL isKeyFrame;
@end

// 音频帧数据
NS_SWIFT_NAME(AudioFrame)
@interface HorsAVAudioFrame : NSObject
@property (nonatomic, readonly) NSInteger sampleRate;
@property (nonatomic, readonly) NSInteger channels;
@property (nonatomic, readonly) int64_t timestamp; // 毫秒
@end

NS_SWIFT_NAME(DataBypassDelegate)
@protocol HorsAVDataBypassDelegate <NSObject>

@optional
- (void)dataBypass:(HorsAVPlayer *)player 
    didReceiveVideoFrame:(HorsAVVideoFrame *)frame 
                atStage:(HorsAVDataBypassStage)stage;

- (void)dataBypass:(HorsAVPlayer *)player 
    didReceiveAudioFrame:(HorsAVAudioFrame *)frame 
                atStage:(HorsAVDataBypassStage)stage;

@end

// 在 HorsAVPlayer 中添加
@interface HorsAVPlayer (DataBypass)

@property (nonatomic, weak, nullable) id<HorsAVDataBypassDelegate> dataBypassDelegate;

- (void)enableDataBypassForVideo:(BOOL)video audio:(BOOL)audio NS_SWIFT_NAME(enableDataBypass(video:audio:));
- (void)setDataBypassStage:(HorsAVDataBypassStage)stage NS_SWIFT_NAME(setDataBypassStage(_:));

@end

NS_ASSUME_NONNULL_END
```

### 5.4 渲染接口 (HorsAVRenderer)

```objc
// HorsAVRenderer.h
#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>

NS_ASSUME_NONNULL_BEGIN

// 渲染模式
NS_SWIFT_NAME(RenderMode)
typedef NS_ENUM(NSInteger, HorsAVRenderMode) {
    HorsAVRenderModeFill,      // 拉伸填充
    HorsAVRenderModeFit,       // 等比适应
    HorsAVRenderModeFillCrop   // 裁剪填充
} NS_SWIFT_NAME(RenderMode);

NS_SWIFT_NAME(PlayerRenderer)
@interface HorsAVPlayerRenderer : NSObject

- (instancetype)initWithView:(MTKView *)view NS_SWIFT_NAME(init(view:)) NS_DESIGNATED_INITIALIZER;

@property (nonatomic, readwrite) HorsAVRenderMode renderMode;
@property (nonatomic, readwrite, nullable) CALayer *overlayLayer;

@end

NS_ASSUME_NONNULL_END
```

### 5.5 日志系统 (HorsAVLogger)

```objc
// HorsAVLogger.h
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(LogLevel)
typedef NS_ENUM(NSInteger, HorsAVLogLevel) {
    HorsAVLogLevelTrace,
    HorsAVLogLevelDebug,
    HorsAVLogLevelInfo,
    HorsAVLogLevelWarning,
    HorsAVLogLevelError,
    HorsAVLogLevelOff
} NS_SWIFT_NAME(LogLevel);

NS_SWIFT_NAME(Logger)
@interface HorsAVLogger : NSObject

+ (void)setLogLevel:(HorsAVLogLevel)level NS_SWIFT_NAME(setLevel(_:));
+ (void)setCallback:(void (^)(HorsAVLogLevel level, NSString *message))callback NS_SWIFT_NAME(setCallback(_:));
+ (void)enableConsoleLogging:(BOOL)enable NS_SWIFT_NAME(enableConsoleLogging(_:));

@end

NS_ASSUME_NONNULL_END
```

---

## 6. 模块兼容性

### 6.1 Swift Package Manager

```swift
import HorsAVSDK

// 使用默认配置创建播放器
let player = try Player(configuration: .default())

// 设置委托
player.delegate = self

// 打开并播放
try player.open(URL(string: "https://example.com/video.mp4")!)
player.play()

// 使用 async/await
await player.seek(to: 60.0) { success, actualTime, error in
    if success {
        print("Seeked to \(actualTime)")
    }
}

// SwiftUI 集成
struct VideoPlayerView: UIViewRepresentable {
    let player: Player
    
    func makeUIView(context: Context) -> MTKView {
        let view = MTKView()
        player.setRenderView(view)
        return view
    }
    
    func updateUIView(_ uiView: MTKView, context: Context) {}
}
```

### 6.2 CocoaPods 支持

```ruby
# Podfile
pod 'HorsAVSDK', '~> 1.0'
```

### 6.3 Carthage 支持

```
github "HorsAV/HorsAVSDK" ~> 1.0
```

### 6.4 Framework 模块

```objc
// Objective-C
@import HorsAVSDK;

HorsAVPlayer *player = [[HorsAVPlayer alloc] initWithConfiguration:nil error:&error];
[player openURL:url error:&error];
[player play];
```

```swift
// Swift
import HorsAVSDK

let player = try Player()
try player.open(videoURL)
player.play()
```

---

## 7. 版本管理策略

### 7.1 C++ SDK 与 OC SDK 版本对应

| C++ SDK 版本 | Objective-C SDK 版本 | 状态 |
|-------------|---------------------|------|
| v1.0.0      | v1.0.0              | ✅ 同步 |
| v1.1.0      | v1.1.0              | ✅ 同步 |

### 7.2 版本兼容性规则

1. **主版本号一致**: C++ SDK 1.x 对应 OC SDK 1.x
2. **API 新增**: 次要版本号增加，保持向后兼容
3. **破坏性变更**: 主版本号增加，需同步更新

### 7.3 同步更新触发条件

| 变更类型 | 需要 OC 层更新 | 示例 |
|---------|--------------|------|
| 新增 API | ✅ 是 | `player->SetDataBypass()` 新增 |
| 删除 API | ✅ 是 | 移除旧接口 |
| 修改参数 | ✅ 是 | 参数类型变化 |
| 平台特性 | ❌ 否 | Android Java 层更新 |
| Bug 修复 | ✅ 是（如影响接口） | 内存泄漏修复 |
| 内部优化 | ❌ 否 | 性能优化不改变接口 |

### 7.4 版本检查机制

```objc
// HorsAVPlayer.m 内部实现
+ (void)checkVersionCompatibility {
    // 运行时检查 C++ SDK 版本
    const char* cppVersion = avsdk::GetVersion();
    if (![self isCompatible:cxxVersion]) {
        NSLog(@"Warning: HorsAVSDK version mismatch");
    }
}
```

---

## 8. 示例代码

### 8.1 基础播放

```objc
// Objective-C
#import <HorsAVSDK/HorsAVSDK.h>

@interface PlayerViewController () <HorsAVPlayerDelegate>
@property (strong, nonatomic) HorsAVPlayer *player;
@property (weak, nonatomic) MTKView *renderView;
@end

@implementation PlayerViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    // 配置
    HorsAVPlayerConfiguration *config = [HorsAVPlayerConfiguration defaultConfiguration];
    config.enableHardwareDecoder = YES;
    config.bufferTime = 2.0;
    
    // 创建播放器
    NSError *error;
    self.player = [[HorsAVPlayer alloc] initWithConfiguration:config error:&error];
    if (!self.player) {
        NSLog(@"Failed to create player: %@", error);
        return;
    }
    
    // 设置委托和渲染视图
    self.player.delegate = self;
    [self.player setRenderView:self.renderView];
    
    // 打开并播放
    NSURL *url = [NSURL fileURLWithPath:@"/path/to/video.mp4"];
    if ([self.player openURL:url error:&error]) {
        [self.player play];
    } else {
        NSLog(@"Failed to open: %@", error);
    }
}

// 委托方法
- (void)player:(HorsAVPlayer *)player didChangeState:(HorsAVPlayerState)oldState 
                                            toState:(HorsAVPlayerState)newState {
    NSLog(@"State changed: %zd -> %zd", oldState, newState);
}

- (void)player:(HorsAVPlayer *)player didUpdateProgress:(NSTimeInterval)currentTime 
                                             duration:(NSTimeInterval)duration {
    NSLog(@"Progress: %.2f/%.2f", currentTime, duration);
}

- (void)player:(HorsAVPlayer *)player didEncounterError:(NSError *)error {
    NSLog(@"Error: %@", error.localizedDescription);
}

@end
```

```swift
// Swift
import HorsAVSDK
import MetalKit

class PlayerViewController: UIViewController, PlayerDelegate {
    var player: Player?
    @IBOutlet weak var renderView: MTKView!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        do {
            // 配置并创建
            var config = Player.Configuration.default()
            config.enableHardwareDecoder = true
            config.bufferTime = 2.0
            
            player = try Player(configuration: config)
            player?.delegate = self
            player?.setRenderView(renderView)
            
            // 播放
            let url = URL(fileURLWithPath: "/path/to/video.mp4")
            try player?.open(url)
            player?.play()
            
        } catch {
            print("Error: \(error)")
        }
    }
    
    // PlayerDelegate
    func player(_ player: Player, didChangeState oldState: PlayerState, toState newState: PlayerState) {
        print("State: \(oldState) -> \(newState)")
    }
    
    func player(_ player: Player, didUpdateProgress currentTime: TimeInterval, duration: TimeInterval) {
        let progress = duration > 0 ? currentTime / duration : 0
        print("Progress: \(Int(progress * 100))%")
    }
    
    func player(_ player: Player, didEncounterError error: Error) {
        print("Error: \(error.localizedDescription)")
    }
}
```

### 8.2 SwiftUI 集成

```swift
import SwiftUI
import HorsAVSDK
import MetalKit

struct HorsAVPlayerView: UIViewRepresentable {
    let player: Player
    
    func makeUIView(context: Context) -> MTKView {
        let view = MTKView()
        player.setRenderView(view)
        return view
    }
    
    func updateUIView(_ uiView: MTKView, context: Context) {}
}

struct ContentView: View {
    @StateObject private var viewModel = PlayerViewModel()
    
    var body: some View {
        VStack {
            HorsAVPlayerView(player: viewModel.player)
                .aspectRatio(16/9, contentMode: .fit)
            
            HStack {
                Button(action: viewModel.play) {
                    Image(systemName: "play.fill")
                }
                Button(action: viewModel.pause) {
                    Image(systemName: "pause.fill")
                }
                Slider(value: $viewModel.progress, in: 0...1) { editing in
                    if !editing {
                        viewModel.seek(to: viewModel.progress * viewModel.duration)
                    }
                }
            }
        }
    }
}

@MainActor
class PlayerViewModel: ObservableObject {
    let player: Player
    @Published var progress: Double = 0
    @Published var duration: TimeInterval = 0
    
    init() {
        player = try! Player(configuration: .default())
        setupTimer()
    }
    
    func play() { player.play() }
    func pause() { player.pause() }
    func seek(to time: TimeInterval) { player.seek(to: time, completionHandler: nil) }
    
    private func setupTimer() {
        Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { _ in
            self.progress = self.player.currentTime / max(self.player.duration, 1)
            self.duration = self.player.duration
        }
    }
}
```

---

## 附录 A: 完整头文件清单

```
HorsAVSDK.h (Umbrella Header)
├── HorsAVTypes.h
├── HorsAVError.h
├── HorsAVPlayerConfig.h
├── HorsAVPlayer.h
├── HorsAVRenderer.h
├── HorsAVDataBypass.h
└── HorsAVLogger.h

HorsAVSDK+Metal.h (可选 Metal 扩展)
└── HorsAVMetalRenderer.h
```

---

## 附录 B: 实现文件清单

```
bindings/objc/
├── HorsAVPlayer.mm
├── HorsAVPlayerConfig.mm
├── HorsAVRenderer.mm
├── HorsAVDataBypass.mm
├── HorsAVLogger.mm
└── Internal/
    ├── HorsAVPlayerInternal.h    // C++ 对象持有
    ├── HorsAVCallbackBridge.h     // C++ 回调桥接
    └── HorsAVUtils.hpp           // 类型转换工具
```

---

**文档状态**: ✅ Implemented  
**维护者**: HorsAVSDK Team  
**最后更新**: 2026-04-23
