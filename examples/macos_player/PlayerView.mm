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
    _mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    _mtkView.depthStencilPixelFormat = MTLPixelFormatInvalid;
    // Disable MTKView's automatic rendering - MetalRenderer will handle it
    _mtkView.enableSetNeedsDisplay = NO;
    _mtkView.paused = YES;
    // Don't set delegate - we handle rendering in MetalRenderer
    // _mtkView.delegate = self;

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

- (void)drawInMTKView:(MTKView *)view {
    if ([self.delegate respondsToSelector:@selector(renderInView:)]) {
        [self.delegate renderInView:view];
    }
}

@end
