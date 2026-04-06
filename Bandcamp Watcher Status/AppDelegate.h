//
//  AppDelegate.h
//  Bandcamp Watcher Status
//
//  Created by Nick Blievers on 6/4/2026.
//

#import <Cocoa/Cocoa.h>

@class BCWStatusController;

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property (readonly, nonatomic, strong) BCWStatusController *statusController;

@end

