//
//  BCWServiceMonitor.h
//  Bandcamp Watcher Status
//
//  Service status monitoring via SMAppService and launchd
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

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
@property (readonly, nullable, nonatomic, copy) NSString *lastControlError;

- (instancetype)initWithServiceLabel:(nullable NSString *)label;

// Check current service state (synchronous, may block briefly)
- (BCWServiceState)checkState;

// Convenience accessors
- (BOOL)isEnabled;
- (BOOL)isRunning;
- (NSString *)stateDescription;

// Raw launchd diagnostic for failures that occur after kickstart succeeds.
- (NSString *)serviceDiagnostic;

// Service controls
- (BOOL)startService;
- (BOOL)stopService;
- (BOOL)restartService;

@end

NS_ASSUME_NONNULL_END
