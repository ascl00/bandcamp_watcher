//
//  AppDelegate.m
//  Bandcamp Watcher Status
//
//  Created by Nick Blievers on 6/4/2026.
//

#import "AppDelegate.h"
#import "BCWStatusController.h"

@interface AppDelegate ()
@property (readwrite, nonatomic, strong) BCWStatusController *statusController;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // Create and start status controller
    self.statusController = [[BCWStatusController alloc] init];
    [self.statusController start];
    
    NSLog(@"Bandcamp Watcher Status launched successfully");
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    [self.statusController stop];
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    return YES;
}

@end
