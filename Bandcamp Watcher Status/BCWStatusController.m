//
//  BCWStatusController.m
//  Bandcamp Watcher Status
//
//  Main menu bar controller
//

#import "BCWStatusController.h"
#import "BCWServiceMonitor.h"
#import "BCWStateStore.h"

static const NSTimeInterval kRefreshInterval = 5.0;  // 5 seconds
static const NSInteger kStartActionTag = 120;
static const NSInteger kRestartActionTag = 121;
static const NSInteger kStopActionTag = 122;

@interface BCWStatusController () <NSMenuDelegate>
@property (readwrite, nonatomic, strong) NSStatusItem *statusItem;
@property (strong, nonatomic) BCWServiceMonitor *serviceMonitor;
@property (strong, nonatomic) BCWStateStore *stateStore;
@property (strong, nonatomic) NSTimer *refreshTimer;
@property (readwrite, nonatomic) BOOL isRunning;
@property (nonatomic) BOOL hasUnseenCopiedAlbums;
@property (nullable, nonatomic, strong) NSDate *latestCopiedAlbumAt;
@property (nullable, nonatomic, strong) NSDate *lastSeenCopiedAlbumAt;
@end

@implementation BCWStatusController

- (instancetype)init {
    self = [super init];
    if (self) {
        _serviceMonitor = [[BCWServiceMonitor alloc] init];
        _stateStore = [[BCWStateStore alloc] init];
        _isRunning = NO;
        _hasUnseenCopiedAlbums = NO;
    }
    return self;
}

- (void)start {
    // Create status item
    NSStatusBar *statusBar = [NSStatusBar systemStatusBar];
    self.statusItem = [statusBar statusItemWithLength:NSVariableStatusItemLength];
    
    if (!self.statusItem) {
        NSLog(@"Failed to create status item");
        return;
    }
    
    // Set up icon - use text initially
    NSStatusBarButton *button = self.statusItem.button;
    button.title = @"♫";
    button.toolTip = @"Bandcamp Watcher Status";
    button.target = self;
    button.action = @selector(statusItemClicked:);
    
    // Set up menu
    self.statusItem.menu = [self createMenu];
    
    // Initial refresh
    [self refresh:nil];
    
    // Start timer
    self.refreshTimer = [NSTimer scheduledTimerWithTimeInterval:kRefreshInterval
                                                          target:self
                                                        selector:@selector(refresh:)
                                                        userInfo:nil
                                                         repeats:YES];
    
    self.isRunning = YES;
    NSLog(@"BCWStatusController started");
}

- (void)statusItemClicked:(id)sender {
    [self markCopiedAlbumsAsSeen];
    [self refresh:nil];
}

- (void)stop {
    [self.refreshTimer invalidate];
    self.refreshTimer = nil;
    
    if (self.statusItem) {
        [[NSStatusBar systemStatusBar] removeStatusItem:self.statusItem];
        self.statusItem = nil;
    }
    
    self.isRunning = NO;
}

- (NSMenu *)createMenu {
    NSMenu *menu = [[NSMenu alloc] init];
    menu.autoenablesItems = NO;
    menu.delegate = self;
    
    // Status section
    NSMenuItem *statusHeader = [[NSMenuItem alloc] initWithTitle:@"Status: Checking..."
                                                          action:nil
                                                   keyEquivalent:@""];
    statusHeader.tag = 100;
    statusHeader.enabled = NO;
    [menu addItem:statusHeader];
    
    NSMenuItem *modeItem = [[NSMenuItem alloc] initWithTitle:@"Mode: --"
                                                        action:nil
                                                 keyEquivalent:@""];
    modeItem.tag = 101;
    modeItem.enabled = NO;
    [menu addItem:modeItem];
    
    NSMenuItem *heartbeatItem = [[NSMenuItem alloc] initWithTitle:@"Last seen: --"
                                                           action:nil
                                                    keyEquivalent:@""];
    heartbeatItem.tag = 102;
    heartbeatItem.enabled = NO;
    [menu addItem:heartbeatItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *startItem = [[NSMenuItem alloc] initWithTitle:@"Start Watcher"
                                                        action:@selector(startWatcher:)
                                                 keyEquivalent:@""];
    startItem.tag = kStartActionTag;
    startItem.target = self;
    startItem.hidden = YES;
    [menu addItem:startItem];

    NSMenuItem *restartItem = [[NSMenuItem alloc] initWithTitle:@"Restart Watcher"
                                                          action:@selector(restartWatcher:)
                                                   keyEquivalent:@""];
    restartItem.tag = kRestartActionTag;
    restartItem.target = self;
    restartItem.hidden = YES;
    [menu addItem:restartItem];

    NSMenuItem *stopItem = [[NSMenuItem alloc] initWithTitle:@"Stop Watcher"
                                                       action:@selector(stopWatcher:)
                                                keyEquivalent:@""];
    stopItem.tag = kStopActionTag;
    stopItem.target = self;
    stopItem.hidden = YES;
    [menu addItem:stopItem];

    [menu addItem:[NSMenuItem separatorItem]];

    // Recent albums header
    NSMenuItem *albumsHeader = [[NSMenuItem alloc] initWithTitle:@"Recent Albums"
                                                            action:nil
                                                     keyEquivalent:@""];
    albumsHeader.enabled = NO;
    [menu addItem:albumsHeader];
    
    // Placeholder for albums (tags 200-209)
    for (int i = 0; i < 10; i++) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@""
                                                        action:nil
                                                 keyEquivalent:@""];
        item.tag = 200 + i;
        item.enabled = NO;
        item.hidden = YES;
        [menu addItem:item];
    }
    
    [menu addItem:[NSMenuItem separatorItem]];
    
    // Actions
    NSMenuItem *refreshItem = [[NSMenuItem alloc] initWithTitle:@"Refresh Now"
                                                          action:@selector(refresh:)
                                                   keyEquivalent:@"r"];
    refreshItem.target = self;
    [menu addItem:refreshItem];
    
    NSMenuItem *configItem = [[NSMenuItem alloc] initWithTitle:@"Open Config..."
                                                         action:@selector(openConfig:)
                                                  keyEquivalent:@"c"];
    configItem.target = self;
    [menu addItem:configItem];
    
    NSMenuItem *dbFolderItem = [[NSMenuItem alloc] initWithTitle:@"Open State DB Folder"
                                                           action:@selector(openDBFolder:)
                                                    keyEquivalent:@"d"];
    dbFolderItem.target = self;
    [menu addItem:dbFolderItem];

    NSMenuItem *logsItem = [[NSMenuItem alloc] initWithTitle:@"Show Logs"
                                                       action:@selector(showLogs:)
                                                keyEquivalent:@"l"];
    logsItem.target = self;
    [menu addItem:logsItem];
    
    [menu addItem:[NSMenuItem separatorItem]];
    
    NSMenuItem *quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit"
                                                       action:@selector(quit:)
                                                keyEquivalent:@"q"];
    quitItem.target = self;
    [menu addItem:quitItem];
    
    return menu;
}

- (void)menuWillOpen:(NSMenu *)menu {
    [self markCopiedAlbumsAsSeen];
    [self refresh:nil];
}

- (void)refresh:(nullable id)sender {
    // Check service state
    BCWServiceState serviceState = [self.serviceMonitor checkState];
    BCWRuntimeStatus *runtime = [self.stateStore fetchRuntimeStatus];
    NSArray *albums = [self.stateStore fetchRecentAlbums:10];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [self updateMenuWithServiceState:serviceState runtime:runtime albums:albums];
    });
}

- (void)updateMenuWithServiceState:(BCWServiceState)serviceState
                           runtime:(nullable BCWRuntimeStatus *)runtime
                             albums:(NSArray<BCWAlbumEvent *> *)albums {
    [self updateUnseenAlbumIndicatorWithAlbums:albums];

    NSMenu *menu = self.statusItem.menu;
    
    // Update status header
    NSMenuItem *statusHeader = [menu itemWithTag:100];
    NSString *statusText;
    
    if (serviceState == BCWServiceStateRunning && runtime) {
        statusText = @"Status: Running";
        [self setStatusIconBase:@"♫"];
    } else if (serviceState == BCWServiceStateEnabledNotRunning) {
        statusText = @"Status: Enabled (not running)";
        [self setStatusIconBase:@"♫ ○"];
    } else if (serviceState == BCWServiceStateNotRegistered) {
        statusText = @"Status: Not Installed";
        [self setStatusIconBase:@"♫ ✕"];
    } else {
        statusText = [NSString stringWithFormat:@"Status: %@", [self.serviceMonitor stateDescription]];
        [self setStatusIconBase:@"♫ ?"];
    }
    statusHeader.title = statusText;
    [self updateServiceActionsForState:serviceState menu:menu];
    
    // Update mode
    NSMenuItem *modeItem = [menu itemWithTag:101];
    if (runtime) {
        modeItem.title = [NSString stringWithFormat:@"Mode: %@", runtime.mode];
        modeItem.hidden = NO;
    } else {
        modeItem.hidden = YES;
    }
    
    // Update heartbeat
    NSMenuItem *heartbeatItem = [menu itemWithTag:102];
    if (runtime && runtime.heartbeatAt) {
        NSTimeInterval ago = [runtime secondsSinceHeartbeat];
        NSString *timeStr;
        if (ago < 60) {
            timeStr = [NSString stringWithFormat:@"%.0fs ago", ago];
        } else if (ago < 3600) {
            timeStr = [NSString stringWithFormat:@"%.0fm ago", ago / 60];
        } else {
            timeStr = [NSString stringWithFormat:@"%.0fh ago", ago / 3600];
        }
        heartbeatItem.title = [NSString stringWithFormat:@"Last seen: %@", timeStr];
        heartbeatItem.hidden = NO;
    } else {
        heartbeatItem.hidden = YES;
    }
    
    // Update albums
    // Hide all album items first
    for (int i = 0; i < 10; i++) {
        NSMenuItem *item = [menu itemWithTag:200 + i];
        item.hidden = YES;
        item.title = @"";
    }
    
    // Show albums (including errors)
    if (albums.count > 0) {
        [albums enumerateObjectsUsingBlock:^(BCWAlbumEvent *album, NSUInteger idx, BOOL *stop) {
            if (idx >= 10) return;
            
            NSMenuItem *item = [menu itemWithTag:(int)(200 + idx)];
            NSString *sourceIcon = (album.sourceType == BCWSourceTypeQobuz) ? @"Q" : @"B";
            NSString *statusIcon = (album.eventType == BCWEventTypeAlbumCopied) ? @"" : @"!";
            item.title = [NSString stringWithFormat:@"  [%@%@] %@ - %@ (%@)",
                          sourceIcon, statusIcon, album.artist, album.album, album.fileType];
            item.hidden = NO;
        }];
    } else {
        // Show "No recent albums" message
        NSMenuItem *item = [menu itemWithTag:200];
        item.title = @"  No recent albums";
        item.hidden = NO;
    }
}

- (void)setStatusIconBase:(NSString *)baseIcon {
    NSString *indicator = self.hasUnseenCopiedAlbums ? @" •" : @"";
    self.statusItem.button.title = [baseIcon stringByAppendingString:indicator];
}

- (void)markCopiedAlbumsAsSeen {
    self.lastSeenCopiedAlbumAt = self.latestCopiedAlbumAt;
    self.hasUnseenCopiedAlbums = NO;
}

- (nullable NSDate *)latestCopiedTimestampInAlbums:(NSArray<BCWAlbumEvent *> *)albums {
    NSDate *latest = nil;
    for (BCWAlbumEvent *album in albums) {
        if (album.eventType != BCWEventTypeAlbumCopied) {
            continue;
        }
        if (!latest || [album.timestamp compare:latest] == NSOrderedDescending) {
            latest = album.timestamp;
        }
    }
    return latest;
}

- (void)updateUnseenAlbumIndicatorWithAlbums:(NSArray<BCWAlbumEvent *> *)albums {
    self.latestCopiedAlbumAt = [self latestCopiedTimestampInAlbums:albums];

    if (!self.latestCopiedAlbumAt) {
        self.hasUnseenCopiedAlbums = NO;
        return;
    }

    if (!self.lastSeenCopiedAlbumAt) {
        self.lastSeenCopiedAlbumAt = self.latestCopiedAlbumAt;
        self.hasUnseenCopiedAlbums = NO;
        return;
    }

    self.hasUnseenCopiedAlbums =
        ([self.latestCopiedAlbumAt compare:self.lastSeenCopiedAlbumAt] == NSOrderedDescending);
}

- (void)updateServiceActionsForState:(BCWServiceState)serviceState menu:(NSMenu *)menu {
    NSMenuItem *startItem = [menu itemWithTag:kStartActionTag];
    NSMenuItem *restartItem = [menu itemWithTag:kRestartActionTag];
    NSMenuItem *stopItem = [menu itemWithTag:kStopActionTag];

    startItem.hidden = YES;
    restartItem.hidden = YES;
    stopItem.hidden = YES;
    startItem.enabled = YES;
    restartItem.enabled = YES;
    stopItem.enabled = YES;
    startItem.title = @"Start Watcher";

    if (serviceState == BCWServiceStateRunning) {
        restartItem.hidden = NO;
        stopItem.hidden = NO;
    } else if (serviceState == BCWServiceStateEnabledNotRunning) {
        startItem.hidden = NO;
    } else if (serviceState == BCWServiceStateNotRegistered) {
        startItem.hidden = NO;
        startItem.enabled = NO;
        startItem.title = @"Start Watcher (service not installed)";
    } else {
        startItem.hidden = NO;
        startItem.enabled = NO;
        startItem.title = @"Start Watcher (unavailable)";
    }
}

#pragma mark - Actions

- (void)openConfig:(nullable id)sender {
    dispatch_async(dispatch_get_main_queue(), ^{
        // Resolve config path using same logic as daemon
        NSString *configPath = nil;
        
        // Try XDG_CONFIG_HOME
        const char *xdgConfig = getenv("XDG_CONFIG_HOME");
        if (xdgConfig) {
            configPath = [[NSString stringWithUTF8String:xdgConfig]
                          stringByAppendingPathComponent:@"bandcamp_watcher/config"];
        } else {
            // Fallback to ~/.config
            configPath = [NSHomeDirectory()
                          stringByAppendingPathComponent:@".config/bandcamp_watcher/config"];
        }
        
        // Check if file exists, open directory instead if not
        NSFileManager *fm = [NSFileManager defaultManager];
        NSString *openPath = configPath;
        if (![fm fileExistsAtPath:configPath]) {
            openPath = [configPath stringByDeletingLastPathComponent];
        }
        
        NSLog(@"Opening config at: %@", openPath);
        
        NSURL *url = [NSURL fileURLWithPath:openPath];
        [[NSWorkspace sharedWorkspace] openURL:url];
    });
}

- (void)startWatcher:(nullable id)sender {
    if (![self.serviceMonitor startService]) {
        [self showServiceControlFailureForAction:@"Start"];
        [self refresh:nil];
        return;
    }
    [self refresh:nil];

    // kickstart only confirms that launchd accepted the request. The process may
    // still exit immediately, before it creates either watcher log file.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        BCWServiceState state = [self.serviceMonitor checkState];
        if (state != BCWServiceStateRunning) {
            NSString *diagnostic = [self.serviceMonitor serviceDiagnostic];
            NSLog(@"Watcher did not remain running after kickstart: %@", diagnostic);
            [self showServiceControlFailureForAction:@"Start" diagnostic:diagnostic];
        }
        [self refresh:nil];
    });
}

- (void)stopWatcher:(nullable id)sender {
    if (![self.serviceMonitor stopService]) {
        [self showServiceControlFailureForAction:@"Stop"];
    }
    [self refresh:nil];
}

- (void)restartWatcher:(nullable id)sender {
    if (![self.serviceMonitor restartService]) {
        [self showServiceControlFailureForAction:@"Restart"];
    }
    [self refresh:nil];
}

- (void)showServiceControlFailureForAction:(NSString *)action {
    NSString *diagnostic = self.serviceMonitor.lastControlError;
    if (diagnostic.length == 0) {
        diagnostic = @"launchctl did not provide an error message.";
    }
    [self showServiceControlFailureForAction:action diagnostic:diagnostic];
}

- (void)showServiceControlFailureForAction:(NSString *)action
                                diagnostic:(NSString *)diagnostic {

    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = [NSString stringWithFormat:@"Couldn’t %@ Watcher", action];
    alert.informativeText = [NSString stringWithFormat:
        @"%@\n\nThe watcher may not have started, so its log files might not contain this failure.",
        diagnostic];
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Show Logs"];

    if ([alert runModal] == NSAlertSecondButtonReturn) {
        [self showLogs:nil];
    }
}

- (void)openDBFolder:(nullable id)sender {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *dbPath = self.stateStore.dbPath;
        NSString *dbFolder = [dbPath stringByDeletingLastPathComponent];
        
        NSLog(@"Opening DB folder at: %@", dbFolder);
        
        NSURL *url = [NSURL fileURLWithPath:dbFolder];
        [[NSWorkspace sharedWorkspace] openURL:url];
    });
}

- (void)showLogs:(nullable id)sender {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *logsFolder = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Logs"];
        NSArray<NSString *> *logPaths = @[
            [logsFolder stringByAppendingPathComponent:@"bandcamp_watcher.log"],
            [logsFolder stringByAppendingPathComponent:@"bandcamp_watcher.error.log"]
        ];

        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSMutableArray<NSURL *> *existingLogs = [NSMutableArray array];
        for (NSString *path in logPaths) {
            if ([fileManager fileExistsAtPath:path]) {
                [existingLogs addObject:[NSURL fileURLWithPath:path]];
            }
        }

        if (existingLogs.count > 0) {
            NSLog(@"Showing watcher logs: %@", existingLogs);
            [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:existingLogs];
            return;
        }

        NSLog(@"Watcher log files do not exist yet; opening logs folder: %@", logsFolder);
        [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:logsFolder]];
    });
}

- (void)quit:(nullable id)sender {
    [self stop];
    [NSApp terminate:self];
}

@end
