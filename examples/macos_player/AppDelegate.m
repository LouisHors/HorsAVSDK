#import "AppDelegate.h"
#import "ViewController.h"

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // Create window
    NSRect frame = NSMakeRect(0, 0, 1280, 720);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:NSWindowStyleMaskTitled
                                                    | NSWindowStyleMaskClosable
                                                    | NSWindowStyleMaskMiniaturizable
                                                    | NSWindowStyleMaskResizable
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
    self.window.title = @"HorsAVPlayer";
    self.window.minSize = NSMakeSize(640, 480);
    self.window.releasedWhenClosed = NO;
    [self.window center];

    // Create and set view controller
    self.viewController = [[ViewController alloc] initWithNibName:nil bundle:nil];
    self.window.contentViewController = self.viewController;

    [self.window makeKeyAndOrderFront:self];

    // Ensure app is active
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end
