//
//  main.m
//  Bandcamp Watcher Status
//
//  Created by Nick Blievers on 6/4/2026.
//

#import <Cocoa/Cocoa.h>
#import "AppDelegate.h"

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create and set up the application and delegate manually
        // (since we're not using a storyboard)
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        app.delegate = delegate;
        
        // Run the application
        [app run];
    }
    return 0;
}
