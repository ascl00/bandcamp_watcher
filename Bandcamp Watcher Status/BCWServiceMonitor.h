//
//  BCWServiceMonitor.h
//  Bandcamp Watcher Status
//
//  Service status monitoring via SMAppService and launchd
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, BCWServiceState) {
    BCWServiceStateUnknown = 0,
    BCWServiceStateNotRegistered,
    BCWServiceStateEnabledNotRunning,
    BCWServiceStateRunning,
    BCWServiceStateDisabled,
    BCWServiceStateError
};

@interface BCWServiceMonitor : NSObject

@property (readonly, nonatomic) BCWServiceState currentState;
@property (readonly, nonatomic, copy) NSString *serviceLabel;

- (instancetype)initWithServiceLabel:(NSString *)label;

// Check current service state (synchronous, may block briefly)
- (BCWServiceState)checkState;

// Convenience accessors
- (BOOL)isEnabled;
- (BOOL)isRunning;
- (NSString *)stateDescription;

@end
