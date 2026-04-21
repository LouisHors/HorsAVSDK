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
