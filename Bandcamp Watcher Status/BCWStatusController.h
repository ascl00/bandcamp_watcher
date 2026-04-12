//
//  BCWStatusController.h
//  Bandcamp Watcher Status
//
//  Main menu bar controller
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface BCWStatusController : NSObject

@property (readonly, nonatomic, strong) NSStatusItem *statusItem;
@property (readonly, nonatomic) BOOL isRunning;

- (void)start;
- (void)stop;

// Menu actions
- (void)refresh:(nullable id)sender;
- (void)startWatcher:(nullable id)sender;
- (void)stopWatcher:(nullable id)sender;
- (void)restartWatcher:(nullable id)sender;
- (void)openConfig:(nullable id)sender;
- (void)openDBFolder:(nullable id)sender;
- (void)quit:(nullable id)sender;

@end

NS_ASSUME_NONNULL_END
