#import "HorsAVRenderer.h"

// MARK: - HorsAVPlayerRenderer Implementation

@interface HorsAVPlayerRenderer ()
@property (nonatomic, strong) MTKView *view;
@end

@implementation HorsAVPlayerRenderer

- (instancetype)initWithView:(MTKView *)view {
    self = [super init];
    if (self) {
        _view = view;
        _renderMode = HorsAVRenderModeFit;
        _enableHDR = NO;
        _backgroundColor = [NSColor blackColor];
        _videoSize = CGSizeZero;
        _displaySize = view.bounds.size;
    }
    return self;
}

- (instancetype)init {
    return nil; // Must use initWithView:
}

- (void)setRenderMode:(HorsAVRenderMode)renderMode {
    _renderMode = renderMode;
    // Update Metal render settings based on mode
}

- (nullable NSImage *)takeScreenshot {
    // Implementation would capture current Metal texture
    return nil;
}

@end
