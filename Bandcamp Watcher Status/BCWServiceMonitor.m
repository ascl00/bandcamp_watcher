//
//  BCWServiceMonitor.m
//  Bandcamp Watcher Status
//
//  Service status monitoring via launchctl
//

#import "BCWServiceMonitor.h"

static NSString * const kLaunchdLabel = @"launched.bandcamp_watcher";

@interface BCWServiceMonitor ()
@property (readwrite, nonatomic) BCWServiceState currentState;
@end

@implementation BCWServiceMonitor

- (NSString *)serviceTarget {
    uid_t uid = getuid();
    return [NSString stringWithFormat:@"gui/%d/%@", uid, self.serviceLabel];
}

- (BOOL)runLaunchctlWithArguments:(NSArray<NSString *> *)arguments {
    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:@"/bin/launchctl"];
    task.arguments = arguments;
    task.standardOutput = [NSPipe pipe];
    task.standardError = [NSPipe pipe];

    NSError *error = nil;
    BOOL launched = [task launchAndReturnError:&error];
    if (!launched) {
        return NO;
    }

    [task waitUntilExit];
    return task.terminationStatus == 0;
}

- (instancetype)initWithServiceLabel:(NSString *)label {
    self = [super init];
    if (self) {
        _serviceLabel = label ?: kLaunchdLabel;
        _currentState = BCWServiceStateUnknown;
    }
    return self;
}

- (instancetype)init {
    return [self initWithServiceLabel:kLaunchdLabel];
}

- (BCWServiceState)checkState {
    // Use launchctl to check service state (reliable method)
    BCWServiceState state = [self checkLaunchctlState];
    self.currentState = state;
    return state;
}

- (BCWServiceState)checkLaunchctlState {
    // Query launchctl print for the specific service
    NSString *serviceTarget = [self serviceTarget];
    
    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:@"/bin/launchctl"];
    task.arguments = @[@"print", serviceTarget];
    
    NSPipe *outputPipe = [NSPipe pipe];
    task.standardOutput = outputPipe;
    task.standardError = [NSPipe pipe];  // Discard errors
    
    NSError *error = nil;
    BOOL launched = [task launchAndReturnError:&error];
    
    if (!launched) {
        return BCWServiceStateError;
    }
    
    [task waitUntilExit];
    
    NSData *outputData = [[outputPipe fileHandleForReading] readDataToEndOfFile];
    NSString *output = [[NSString alloc] initWithData:outputData encoding:NSUTF8StringEncoding];
    
    // Check for "not found" or similar
    if (task.terminationStatus != 0 || [output containsString:@"not found"]) {
        return BCWServiceStateNotRegistered;
    }
    
    // Parse output for state information
    // Look for "state = running" or PID indicators
    if ([output containsString:@"state = running"] ||
        [output containsString:@"pid = "]) {
        return BCWServiceStateRunning;
    }
    
    // Service exists but not running
    if ([output containsString:self.serviceLabel]) {
        return BCWServiceStateEnabledNotRunning;
    }
    
    return BCWServiceStateUnknown;
}

- (BOOL)startService {
    BOOL ok = [self runLaunchctlWithArguments:@[@"kickstart", [self serviceTarget]]];
    [self checkState];
    return ok;
}

- (BOOL)stopService {
    BOOL ok = [self runLaunchctlWithArguments:@[@"stop", [self serviceTarget]]];
    [self checkState];
    return ok;
}

- (BOOL)restartService {
    BOOL ok = [self runLaunchctlWithArguments:@[@"kickstart", @"-k", [self serviceTarget]]];
    [self checkState];
    return ok;
}

- (BOOL)isEnabled {
    BCWServiceState state = [self checkState];
    return (state == BCWServiceStateRunning ||
            state == BCWServiceStateEnabledNotRunning);
}

- (BOOL)isRunning {
    BCWServiceState state = [self checkState];
    return (state == BCWServiceStateRunning);
}

- (NSString *)stateDescription {
    switch (self.currentState) {
        case BCWServiceStateUnknown:
            return @"Unknown";
        case BCWServiceStateNotRegistered:
            return @"Not Installed";
        case BCWServiceStateEnabledNotRunning:
            return @"Enabled (Not Running)";
        case BCWServiceStateRunning:
            return @"Running";
        case BCWServiceStateDisabled:
            return @"Disabled";
        case BCWServiceStateError:
            return @"Error";
        default:
            return @"Unknown";
    }
}

@end
