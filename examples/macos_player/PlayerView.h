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
