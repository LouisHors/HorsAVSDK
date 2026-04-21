#import "ViewController.h"
#import <AVFoundation/AVFoundation.h>

@interface ViewController ()
@property (strong) NSTimer *progressTimer;
@property (assign) BOOL isPlaying;
@end

@implementation ViewController

#pragma mark - Lifecycle

- (void)viewDidLoad {
    [super viewDidLoad];

    [self setupUI];
    [self setupPlayer];
}

- (void)viewWillDisappear {
    [super viewWillDisappear];

    [self.progressTimer invalidate];
    self.progressTimer = nil;

    [self.player releasePlayer];
    self.player = nil;
}

#pragma mark - UI Setup

- (void)setupUI {
    // Set black background
    self.view.wantsLayer = YES;
    self.view.layer.backgroundColor = [NSColor blackColor].CGColor;

    // Create PlayerView (fills window)
    self.playerView = [[PlayerView alloc] initWithFrame:self.view.bounds];
    self.playerView.translatesAutoresizingMaskIntoConstraints = NO;
    self.playerView.delegate = self;
    [self.view addSubview:self.playerView];

    [NSLayoutConstraint activateConstraints:@[
        [self.playerView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.playerView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.playerView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [self.playerView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor]
    ]];

    // Create controls overlay at bottom
    NSView *controlsView = [[NSView alloc] init];
    controlsView.translatesAutoresizingMaskIntoConstraints = NO;
    controlsView.wantsLayer = YES;
    controlsView.layer.backgroundColor = [NSColor colorWithWhite:0.0 alpha:0.7].CGColor;
    [self.view addSubview:controlsView];

    [NSLayoutConstraint activateConstraints:@[
        [controlsView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [controlsView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [controlsView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [controlsView.heightAnchor constraintEqualToConstant:80]
    ]];

    // Open button
    self.openButton = [[NSButton alloc] init];
    self.openButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.openButton setTitle:@"Open"];
    [self.openButton setTarget:self];
    [self.openButton setAction:@selector(openFile:)];
    [self.openButton setBezelStyle:NSBezelStyleRounded];
    [controlsView addSubview:self.openButton];

    // Play/Pause button
    self.playPauseButton = [[NSButton alloc] init];
    self.playPauseButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.playPauseButton setTitle:@"Play"];
    [self.playPauseButton setTarget:self];
    [self.playPauseButton setAction:@selector(togglePlayPause:)];
    [self.playPauseButton setBezelStyle:NSBezelStyleRounded];
    self.playPauseButton.enabled = NO;
    [controlsView addSubview:self.playPauseButton];

    // Stop button
    self.stopButton = [[NSButton alloc] init];
    self.stopButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.stopButton setTitle:@"Stop"];
    [self.stopButton setTarget:self];
    [self.stopButton setAction:@selector(stop:)];
    [self.stopButton setBezelStyle:NSBezelStyleRounded];
    self.stopButton.enabled = NO;
    [controlsView addSubview:self.stopButton];

    // Decoder mode popup
    self.decoderModePopup = [[NSPopUpButton alloc] init];
    self.decoderModePopup.translatesAutoresizingMaskIntoConstraints = NO;
    [self.decoderModePopup addItemWithTitle:@"Auto"];
    [self.decoderModePopup addItemWithTitle:@"Software"];
    [self.decoderModePopup addItemWithTitle:@"Hardware"];
    [self.decoderModePopup addItemWithTitle:@"Hardware First"];
    [self.decoderModePopup setTarget:self];
    [self.decoderModePopup setAction:@selector(decoderModeChanged:)];
    [controlsView addSubview:self.decoderModePopup];

    // Progress slider
    self.progressSlider = [[NSSlider alloc] init];
    self.progressSlider.translatesAutoresizingMaskIntoConstraints = NO;
    self.progressSlider.minValue = 0;
    self.progressSlider.maxValue = 1;
    self.progressSlider.doubleValue = 0;
    self.progressSlider.enabled = NO;
    [self.progressSlider setTarget:self];
    [self.progressSlider setAction:@selector(sliderChanged:)];
    [controlsView addSubview:self.progressSlider];

    // Time label
    self.timeLabel = [[NSTextField alloc] init];
    self.timeLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.timeLabel.editable = NO;
    self.timeLabel.bordered = NO;
    self.timeLabel.backgroundColor = [NSColor clearColor];
    self.timeLabel.textColor = [NSColor whiteColor];
    self.timeLabel.alignment = NSTextAlignmentRight;
    [self.timeLabel setStringValue:@"00:00 / 00:00"];
    [controlsView addSubview:self.timeLabel];

    // Layout constraints for controls
    [NSLayoutConstraint activateConstraints:@[
        // Open button (left)
        [self.openButton.leadingAnchor constraintEqualToAnchor:controlsView.leadingAnchor constant:20],
        [self.openButton.centerYAnchor constraintEqualToAnchor:controlsView.centerYAnchor],
        [self.openButton.widthAnchor constraintEqualToConstant:60],

        // Play/Pause button (next to Open)
        [self.playPauseButton.leadingAnchor constraintEqualToAnchor:self.openButton.trailingAnchor constant:10],
        [self.playPauseButton.centerYAnchor constraintEqualToAnchor:controlsView.centerYAnchor],
        [self.playPauseButton.widthAnchor constraintEqualToConstant:80],

        // Stop button (next to Play)
        [self.stopButton.leadingAnchor constraintEqualToAnchor:self.playPauseButton.trailingAnchor constant:10],
        [self.stopButton.centerYAnchor constraintEqualToAnchor:controlsView.centerYAnchor],
        [self.stopButton.widthAnchor constraintEqualToConstant:60],

        // Decoder mode popup (next to Stop)
        [self.decoderModePopup.leadingAnchor constraintEqualToAnchor:self.stopButton.trailingAnchor constant:10],
        [self.decoderModePopup.centerYAnchor constraintEqualToAnchor:controlsView.centerYAnchor],
        [self.decoderModePopup.widthAnchor constraintEqualToConstant:140],

        // Time label (right)
        [self.timeLabel.trailingAnchor constraintEqualToAnchor:controlsView.trailingAnchor constant:-20],
        [self.timeLabel.centerYAnchor constraintEqualToAnchor:controlsView.centerYAnchor],
        [self.timeLabel.widthAnchor constraintEqualToConstant:120],

        // Progress slider (center, with margin)
        [self.progressSlider.leadingAnchor constraintEqualToAnchor:self.decoderModePopup.trailingAnchor constant:20],
        [self.progressSlider.trailingAnchor constraintEqualToAnchor:self.timeLabel.leadingAnchor constant:-20],
        [self.progressSlider.centerYAnchor constraintEqualToAnchor:controlsView.centerYAnchor]
    ]];
}

#pragma mark - Player Setup

- (void)setupPlayer {
    self.player = [[PlayerWrapper alloc] init];
    self.player.delegate = self;

    // Initialize player and set render view
    [self.player initializePlayer];
    [self.player setRenderView:self.playerView.mtkView];
}

#pragma mark - Actions

- (void)openFile:(id)sender {
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    openPanel.allowedFileTypes = @[@"mp4", @"mov", @"avi", @"mkv", @"flv", @"wmv", @"m4v", @"3gp", @"mpg", @"mpeg", @"m4a", @"mp3"];
    openPanel.allowsMultipleSelection = NO;
    openPanel.canChooseDirectories = NO;
    openPanel.canChooseFiles = YES;

    [openPanel beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSURL *url = openPanel.URL;
            [self loadFile:url.path];
        }
    }];
}

- (void)loadFile:(NSString *)path {
    // Get current decoder mode
    NSInteger selectedIndex = self.decoderModePopup.indexOfSelectedItem;
    switch (selectedIndex) {
        case 0: self.player.decoderMode = DecoderModeAuto; break;
        case 1: self.player.decoderMode = DecoderModeSoftware; break;
        case 2: self.player.decoderMode = DecoderModeHardware; break;
        case 3: self.player.decoderMode = DecoderModeHardwareFirst; break;
    }

    // Open file (render view already set in setupPlayer)
    BOOL success = [self.player openFile:path];
    if (success) {
        self.isPlaying = YES;
        [self.playPauseButton setTitle:@"Pause"];
        self.playPauseButton.enabled = YES;
        self.stopButton.enabled = YES;
        self.progressSlider.enabled = YES;

        // Start playing
        [self.player play];

        // Start progress timer
        [self startProgressTimer];
    } else {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Error"];
        [alert setInformativeText:[NSString stringWithFormat:@"Failed to open file: %@", path]];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert runModal];
    }
}

- (void)togglePlayPause:(id)sender {
    if (self.isPlaying) {
        [self.player pause];
        self.isPlaying = NO;
        [self.playPauseButton setTitle:@"Play"];
    } else {
        [self.player play];
        self.isPlaying = YES;
        [self.playPauseButton setTitle:@"Pause"];
    }
}

- (void)stop:(id)sender {
    [self.player stop];
    self.isPlaying = NO;
    [self.playPauseButton setTitle:@"Play"];
    self.progressSlider.doubleValue = 0;
    [self updateTimeLabelWithPosition:0 duration:0];

    [self.progressTimer invalidate];
    self.progressTimer = nil;
}

- (void)sliderChanged:(id)sender {
    double progress = self.progressSlider.doubleValue;
    NSTimeInterval duration = [self.player duration];
    NSTimeInterval position = progress * duration;
    [self.player seekTo:position];
}

- (void)decoderModeChanged:(id)sender {
    // Reinitialize player with new decoder mode if a file is loaded
    // This will be applied on next file open
    if (self.player) {
        // For now, we need to reload the current file
        // In a full implementation, we might want to handle this more gracefully
    }
}

#pragma mark - Progress Timer

- (void)startProgressTimer {
    [self.progressTimer invalidate];
    self.progressTimer = [NSTimer scheduledTimerWithTimeInterval:0.5
                                                          target:self
                                                        selector:@selector(updateProgress)
                                                        userInfo:nil
                                                         repeats:YES];
}

- (void)updateProgress {
    if (!self.player) return;

    NSTimeInterval position = [self.player currentTime];
    NSTimeInterval duration = [self.player duration];

    if (duration > 0) {
        double progress = position / duration;
        self.progressSlider.doubleValue = progress;
        [self updateTimeLabelWithPosition:position duration:duration];

        // Add audio buffer info to time label
        // Get buffered duration from player if available
        NSString *positionStr = [self formatTime:position];
        NSString *durationStr = [self formatTime:duration];
        [self.timeLabel setStringValue:[NSString stringWithFormat:@"%@ / %@ (%.0f%%)",
                                        positionStr, durationStr, progress * 100]];
    }
}

- (NSString *)formatTime:(double)seconds {
    int totalSeconds = (int)seconds;
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;
    return [NSString stringWithFormat:@"%02d:%02d", minutes, secs];
}

- (void)updateTimeLabelWithPosition:(double)position duration:(double)duration {
    NSString *positionStr = [self formatTime:position];
    NSString *durationStr = [self formatTime:duration];
    double progress = duration > 0 ? (position / duration) : 0;
    [self.timeLabel setStringValue:[NSString stringWithFormat:@"%@ / %@ (%.0f%%)", positionStr, durationStr, progress * 100]];
}

#pragma mark - PlayerViewDelegate

- (void)playerView:(PlayerView *)playerView didRequestRender:(CVPixelBufferRef)pixelBuffer {
    // Render callback - PlayerWrapper handles this internally
}

- (void)playerView:(PlayerView *)playerView didChangeSize:(NSSize)size {
    // Handle view size change
}

- (void)playerView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    // Handle drawable size change
}

- (void)renderInView:(MTKView *)view {
    // Render in Metal view - handled by PlayerWrapper
}

#pragma mark - PlayerWrapperDelegate

- (void)playerWrapper:(PlayerWrapper *)wrapper stateChanged:(DemoPlayerState)state {
    dispatch_async(dispatch_get_main_queue(), ^{
        switch (state) {
            case DemoPlayerStateIdle:
            case DemoPlayerStateReady:
                self.isPlaying = NO;
                [self.playPauseButton setTitle:@"Play"];
                self.playPauseButton.enabled = NO;
                self.stopButton.enabled = NO;
                self.progressSlider.enabled = NO;
                break;
            case DemoPlayerStatePlaying:
                self.isPlaying = YES;
                [self.playPauseButton setTitle:@"Pause"];
                self.playPauseButton.enabled = YES;
                self.stopButton.enabled = YES;
                self.progressSlider.enabled = YES;
                break;
            case DemoPlayerStatePaused:
                self.isPlaying = NO;
                [self.playPauseButton setTitle:@"Play"];
                self.playPauseButton.enabled = YES;
                self.stopButton.enabled = YES;
                self.progressSlider.enabled = YES;
                break;
            case DemoPlayerStateError:
            case DemoPlayerStateBuffering:
                self.isPlaying = NO;
                [self.playPauseButton setTitle:@"Play"];
                self.playPauseButton.enabled = NO;
                self.stopButton.enabled = NO;
                self.progressSlider.enabled = NO;
                break;
        }
    });
}

- (void)playerWrapper:(PlayerWrapper *)wrapper didUpdateProgress:(NSTimeInterval)currentTime duration:(NSTimeInterval)duration {
    // Handled by timer
}

- (void)playerWrapper:(PlayerWrapper *)wrapper didEncounterError:(NSError *)error {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Playback Error"];
        [alert setInformativeText:error.localizedDescription];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert runModal];
    });
}

@end
