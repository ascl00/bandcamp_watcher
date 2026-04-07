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

@interface BCWStatusController ()
@property (readwrite, nonatomic, strong) NSStatusItem *statusItem;
@property (strong, nonatomic) BCWServiceMonitor *serviceMonitor;
@property (strong, nonatomic) BCWStateStore *stateStore;
@property (strong, nonatomic) NSTimer *refreshTimer;
@property (readwrite, nonatomic) BOOL isRunning;
@end

@implementation BCWStatusController

- (instancetype)init {
    self = [super init];
    if (self) {
        _serviceMonitor = [[BCWServiceMonitor alloc] init];
        _stateStore = [[BCWStateStore alloc] init];
        _isRunning = NO;
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
    // Force menu to show
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
    
    [menu addItem:[NSMenuItem separatorItem]];
    
    NSMenuItem *quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit"
                                                       action:@selector(quit:)
                                                keyEquivalent:@"q"];
    quitItem.target = self;
    [menu addItem:quitItem];
    
    return menu;
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
    NSMenu *menu = self.statusItem.menu;
    
    // Update status header
    NSMenuItem *statusHeader = [menu itemWithTag:100];
    NSString *statusText;
    
    if (serviceState == BCWServiceStateRunning && runtime) {
        if (runtime.isStale) {
            statusText = @"Status: Running (stale)";
            self.statusItem.button.title = @"♫ ?";
        } else {
            statusText = @"Status: Running";
            self.statusItem.button.title = @"♫";
        }
    } else if (serviceState == BCWServiceStateEnabledNotRunning) {
        statusText = @"Status: Enabled (not running)";
        self.statusItem.button.title = @"♫ ○";
    } else if (serviceState == BCWServiceStateNotRegistered) {
        statusText = @"Status: Not Installed";
        self.statusItem.button.title = @"♫ ✕";
    } else {
        statusText = [NSString stringWithFormat:@"Status: %@", [self.serviceMonitor stateDescription]];
        self.statusItem.button.title = @"♫ ?";
    }
    statusHeader.title = statusText;
    
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

- (void)openDBFolder:(nullable id)sender {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *dbPath = self.stateStore.dbPath;
        NSString *dbFolder = [dbPath stringByDeletingLastPathComponent];
        
        NSLog(@"Opening DB folder at: %@", dbFolder);
        
        NSURL *url = [NSURL fileURLWithPath:dbFolder];
        [[NSWorkspace sharedWorkspace] openURL:url];
    });
}

- (void)quit:(nullable id)sender {
    [self stop];
    [NSApp terminate:self];
}

@end
