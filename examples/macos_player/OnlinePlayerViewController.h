#import <Cocoa/Cocoa.h>
#import "PlayerWrapper.h"

@interface OnlinePlayerViewController : NSViewController <PlayerWrapperDelegate>

@property (strong) NSView *playerView;
@property (strong) PlayerWrapper *player;

// UI Controls
@property (strong) NSTextField *urlTextField;
@property (strong) NSPopUpButton *protocolPopup;
@property (strong) NSButton *playButton;
@property (strong) NSButton *stopButton;
@property (strong) NSButton *backButton;
@property (strong) NSTextField *statusLabel;
@property (strong) NSPopUpButton *audioTrackPopup;
@property (strong) NSSlider *progressSlider;
@property (strong) NSTextField *timeLabel;

@end
