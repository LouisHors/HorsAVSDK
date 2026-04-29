#import "OnlinePlayerViewController.h"
#import "ViewController.h"

@interface OnlinePlayerViewController ()
@property (assign) BOOL isPlaying;
@end

@implementation OnlinePlayerViewController

#pragma mark - Lifecycle

- (void)viewDidLoad {
    [super viewDidLoad];
    [self setupUI];
    [self setupPlayer];
}

- (void)viewWillDisappear {
    [super viewWillDisappear];
    [self.player releasePlayer];
    self.player = nil;
}

#pragma mark - UI Setup

- (void)setupUI {
    self.view.wantsLayer = YES;
    self.view.layer.backgroundColor = [NSColor blackColor].CGColor;

    // Top toolbar with URL input and controls
    NSView *toolbarView = [[NSView alloc] init];
    toolbarView.translatesAutoresizingMaskIntoConstraints = NO;
    toolbarView.wantsLayer = YES;
    toolbarView.layer.backgroundColor = [NSColor colorWithWhite:0.1 alpha:1.0].CGColor;
    [self.view addSubview:toolbarView];

    [NSLayoutConstraint activateConstraints:@[
        [toolbarView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [toolbarView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [toolbarView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [toolbarView.heightAnchor constraintEqualToConstant:60]
    ]];

    // Back button
    self.backButton = [[NSButton alloc] init];
    self.backButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.backButton setTitle:@"< Local"];
    [self.backButton setTarget:self];
    [self.backButton setAction:@selector(backToLocal:)];
    [self.backButton setBezelStyle:NSBezelStyleRounded];
    [toolbarView addSubview:self.backButton];

    // Protocol popup
    self.protocolPopup = [[NSPopUpButton alloc] init];
    self.protocolPopup.translatesAutoresizingMaskIntoConstraints = NO;
    [self.protocolPopup addItemWithTitle:@"HTTP/HTTPS"];
    [self.protocolPopup addItemWithTitle:@"HLS (.m3u8)"];
    [self.protocolPopup addItemWithTitle:@"FLV"];
    [self.protocolPopup addItemWithTitle:@"RTMP"];
    [self.protocolPopup setTarget:self];
    [self.protocolPopup setAction:@selector(protocolChanged:)];
    [toolbarView addSubview:self.protocolPopup];

    // URL input field
    self.urlTextField = [[NSTextField alloc] init];
    self.urlTextField.translatesAutoresizingMaskIntoConstraints = NO;
    self.urlTextField.placeholderString = @"Enter stream URL...";
    [toolbarView addSubview:self.urlTextField];

    // Play button
    self.playButton = [[NSButton alloc] init];
    self.playButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.playButton setTitle:@"Play"];
    [self.playButton setTarget:self];
    [self.playButton setAction:@selector(playStream:)];
    [self.playButton setBezelStyle:NSBezelStyleRounded];
    [toolbarView addSubview:self.playButton];

    // Stop button
    self.stopButton = [[NSButton alloc] init];
    self.stopButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.stopButton setTitle:@"Stop"];
    [self.stopButton setTarget:self];
    [self.stopButton setAction:@selector(stopStream:)];
    [self.stopButton setBezelStyle:NSBezelStyleRounded];
    self.stopButton.enabled = NO;
    [toolbarView addSubview:self.stopButton];

    // Audio track selector
    self.audioTrackPopup = [[NSPopUpButton alloc] init];
    self.audioTrackPopup.translatesAutoresizingMaskIntoConstraints = NO;
    [self.audioTrackPopup addItemWithTitle:@"Audio Tracks"];
    [self.audioTrackPopup setTarget:self];
    [self.audioTrackPopup setAction:@selector(audioTrackChanged:)];
    self.audioTrackPopup.enabled = NO;
    [toolbarView addSubview:self.audioTrackPopup];

    // Status label
    self.statusLabel = [[NSTextField alloc] init];
    self.statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusLabel.editable = NO;
    self.statusLabel.bordered = NO;
    self.statusLabel.backgroundColor = [NSColor clearColor];
    self.statusLabel.textColor = [NSColor whiteColor];
    self.statusLabel.alignment = NSTextAlignmentRight;
    [self.statusLabel setStringValue:@"Ready"];
    [toolbarView addSubview:self.statusLabel];

    // Toolbar layout
    [NSLayoutConstraint activateConstraints:@[
        [self.backButton.leadingAnchor constraintEqualToAnchor:toolbarView.leadingAnchor constant:12],
        [self.backButton.centerYAnchor constraintEqualToAnchor:toolbarView.centerYAnchor],
        [self.backButton.widthAnchor constraintEqualToConstant:70],

        [self.protocolPopup.leadingAnchor constraintEqualToAnchor:self.backButton.trailingAnchor constant:12],
        [self.protocolPopup.centerYAnchor constraintEqualToAnchor:toolbarView.centerYAnchor],
        [self.protocolPopup.widthAnchor constraintEqualToConstant:120],

        [self.playButton.leadingAnchor constraintEqualToAnchor:self.protocolPopup.trailingAnchor constant:12],
        [self.playButton.centerYAnchor constraintEqualToAnchor:toolbarView.centerYAnchor],
        [self.playButton.widthAnchor constraintEqualToConstant:60],

        [self.stopButton.leadingAnchor constraintEqualToAnchor:self.playButton.trailingAnchor constant:8],
        [self.stopButton.centerYAnchor constraintEqualToAnchor:toolbarView.centerYAnchor],
        [self.stopButton.widthAnchor constraintEqualToConstant:60],

        [self.audioTrackPopup.leadingAnchor constraintEqualToAnchor:self.stopButton.trailingAnchor constant:8],
        [self.audioTrackPopup.centerYAnchor constraintEqualToAnchor:toolbarView.centerYAnchor],
        [self.audioTrackPopup.widthAnchor constraintEqualToConstant:140],

        [self.statusLabel.trailingAnchor constraintEqualToAnchor:toolbarView.trailingAnchor constant:-12],
        [self.statusLabel.centerYAnchor constraintEqualToAnchor:toolbarView.centerYAnchor],
        [self.statusLabel.widthAnchor constraintEqualToConstant:100],

        [self.urlTextField.leadingAnchor constraintEqualToAnchor:self.audioTrackPopup.trailingAnchor constant:12],
        [self.urlTextField.trailingAnchor constraintEqualToAnchor:self.statusLabel.leadingAnchor constant:-12],
        [self.urlTextField.centerYAnchor constraintEqualToAnchor:toolbarView.centerYAnchor],
        [self.urlTextField.heightAnchor constraintEqualToConstant:28]
    ]];

    // Player view (rendering area)
    self.playerView = [[NSView alloc] initWithFrame:self.view.bounds];
    self.playerView.translatesAutoresizingMaskIntoConstraints = NO;
    self.playerView.wantsLayer = YES;
    self.playerView.layer.backgroundColor = [NSColor colorWithWhite:0.05 alpha:1.0].CGColor;
    [self.view addSubview:self.playerView];

    // Bottom controls
    NSView *controlsView = [[NSView alloc] init];
    controlsView.translatesAutoresizingMaskIntoConstraints = NO;
    controlsView.wantsLayer = YES;
    controlsView.layer.backgroundColor = [NSColor colorWithWhite:0.1 alpha:1.0].CGColor;
    [self.view addSubview:controlsView];

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

    [NSLayoutConstraint activateConstraints:@[
        [toolbarView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [toolbarView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [toolbarView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],

        [self.playerView.topAnchor constraintEqualToAnchor:toolbarView.bottomAnchor],
        [self.playerView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.playerView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],

        [controlsView.topAnchor constraintEqualToAnchor:self.playerView.bottomAnchor],
        [controlsView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [controlsView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [controlsView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [controlsView.heightAnchor constraintEqualToConstant:50],

        [self.progressSlider.leadingAnchor constraintEqualToAnchor:controlsView.leadingAnchor constant:20],
        [self.progressSlider.trailingAnchor constraintEqualToAnchor:self.timeLabel.leadingAnchor constant:-20],
        [self.progressSlider.centerYAnchor constraintEqualToAnchor:controlsView.centerYAnchor],

        [self.timeLabel.trailingAnchor constraintEqualToAnchor:controlsView.trailingAnchor constant:-20],
        [self.timeLabel.centerYAnchor constraintEqualToAnchor:controlsView.centerYAnchor],
        [self.timeLabel.widthAnchor constraintEqualToConstant:120]
    ]];
}

#pragma mark - Player Setup

- (void)setupPlayer {
    self.player = [[PlayerWrapper alloc] init];
    self.player.delegate = self;
    [self.player initializePlayer];
    [self.player setRenderView:self.playerView];
}

#pragma mark - Actions

- (void)backToLocal:(id)sender {
    ViewController *localVC = [[ViewController alloc] initWithNibName:nil bundle:nil];
    NSWindow *window = self.view.window;
    window.contentViewController = localVC;
}

- (void)protocolChanged:(id)sender {
    NSInteger index = self.protocolPopup.indexOfSelectedItem;
    switch (index) {
        case 0: self.urlTextField.placeholderString = @"https://example.com/video.mp4"; break;
        case 1: self.urlTextField.placeholderString = @"https://example.com/playlist.m3u8"; break;
        case 2: self.urlTextField.placeholderString = @"https://example.com/stream.flv"; break;
        case 3: self.urlTextField.placeholderString = @"rtmp://example.com/live/stream"; break;
    }
}

- (void)playStream:(id)sender {
    NSString *urlString = self.urlTextField.stringValue;
    if (urlString.length == 0) {
        // Use placeholder as hint
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Empty URL"];
        [alert setInformativeText:@"Please enter a stream URL"];
        [alert setAlertStyle:NSAlertStyleWarning];
        [alert runModal];
        return;
    }

    [self.player stop];

    NSURL *url = [NSURL URLWithString:urlString];
    if (!url) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Invalid URL"];
        [alert setInformativeText:@"The entered URL is not valid"];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert runModal];
        return;
    }

    [self.statusLabel setStringValue:@"Opening..."];

    BOOL success = [self.player openURL:url];
    if (success) {
        self.isPlaying = YES;
        [self.playButton setTitle:@"Pause"];
        self.stopButton.enabled = YES;
        self.progressSlider.enabled = YES;
        [self.player play];
    } else {
        [self.statusLabel setStringValue:@"Open Failed"];
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Playback Error"];
        [alert setInformativeText:[NSString stringWithFormat:@"Failed to open: %@", urlString]];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert runModal];
    }
}

- (void)stopStream:(id)sender {
    [self.player stop];
    self.isPlaying = NO;
    [self.playButton setTitle:@"Play"];
    self.stopButton.enabled = NO;
    self.progressSlider.doubleValue = 0;
    [self updateTimeLabelWithPosition:0 duration:0];
    [self.statusLabel setStringValue:@"Ready"];
}

- (void)sliderChanged:(id)sender {
    double progress = self.progressSlider.doubleValue;
    NSTimeInterval duration = [self.player duration];
    NSTimeInterval position = progress * duration;
    [self.player seekTo:position];
}

- (void)audioTrackChanged:(id)sender {
    NSInteger selectedIndex = self.audioTrackPopup.indexOfSelectedItem;
    if (selectedIndex < 0) return;

    NSArray<HorsAVAudioTrackInfo *> *tracks = self.player.audioTracks;
    if (selectedIndex >= tracks.count) return;

    HorsAVAudioTrackInfo *track = tracks[selectedIndex];
    [self.player selectAudioTrack:track.trackIndex];
    [self.statusLabel setStringValue:[NSString stringWithFormat:@"Track: %@", track.title]];
}

#pragma mark - Helpers

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

#pragma mark - PlayerWrapperDelegate

- (void)playerWrapper:(PlayerWrapper *)wrapper stateChanged:(DemoPlayerState)state {
    dispatch_async(dispatch_get_main_queue(), ^{
        switch (state) {
            case DemoPlayerStateIdle:
            case DemoPlayerStateReady:
                self.isPlaying = NO;
                [self.playButton setTitle:@"Play"];
                self.stopButton.enabled = NO;
                self.progressSlider.enabled = NO;
                [self.statusLabel setStringValue:@"Ready"];
                break;
            case DemoPlayerStatePlaying:
                self.isPlaying = YES;
                [self.playButton setTitle:@"Pause"];
                self.stopButton.enabled = YES;
                self.progressSlider.enabled = YES;
                [self.statusLabel setStringValue:@"Playing"];
                break;
            case DemoPlayerStatePaused:
                self.isPlaying = NO;
                [self.playButton setTitle:@"Play"];
                self.stopButton.enabled = YES;
                self.progressSlider.enabled = YES;
                [self.statusLabel setStringValue:@"Paused"];
                break;
            case DemoPlayerStateBuffering:
                self.isPlaying = NO;
                [self.statusLabel setStringValue:@"Buffering..."];
                break;
            case DemoPlayerStateError:
                self.isPlaying = NO;
                [self.playButton setTitle:@"Play"];
                self.stopButton.enabled = NO;
                self.progressSlider.enabled = NO;
                [self.statusLabel setStringValue:@"Error"];
                break;
        }
    });
}

- (void)playerWrapper:(PlayerWrapper *)wrapper didUpdateProgress:(NSTimeInterval)currentTime duration:(NSTimeInterval)duration {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (duration > 0) {
            double progress = currentTime / duration;
            self.progressSlider.doubleValue = progress;
            [self updateTimeLabelWithPosition:currentTime duration:duration];
        }
    });
}

- (void)playerWrapper:(PlayerWrapper *)wrapper didPrepareMedia:(HorsAVMediaInfo *)info {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.audioTrackPopup removeAllItems];
        if (info.audioTracks.count > 0) {
            for (HorsAVAudioTrackInfo *track in info.audioTracks) {
                NSString *label = track.title.length > 0 ? track.title : [NSString stringWithFormat:@"Track %ld", (long)track.trackIndex];
                [self.audioTrackPopup addItemWithTitle:label];
            }
            self.audioTrackPopup.enabled = YES;
        } else {
            [self.audioTrackPopup addItemWithTitle:@"No Audio"];
            self.audioTrackPopup.enabled = NO;
        }
    });
}

- (void)playerWrapper:(PlayerWrapper *)wrapper didEncounterError:(NSError *)error {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.statusLabel setStringValue:@"Error"];
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Playback Error"];
        [alert setInformativeText:error.localizedDescription];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert runModal];
    });
}

@end
