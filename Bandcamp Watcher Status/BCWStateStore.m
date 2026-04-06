//
//  BCWStateStore.m
//  Bandcamp Watcher Status
//
//  Read-only access to daemon's SQLite state database
//

#import "BCWStateStore.h"
#import <sqlite3.h>

static NSString * const kDefaultDBRelativePath = @"Library/Application Support/bandcamp_watcher/state.sqlite3";
static NSString * const kXDGStateRelativePath = @".local/state/bandcamp_watcher/state.sqlite3";
static const NSTimeInterval kStaleThreshold = 60.0;  // 60 seconds

@implementation BCWAlbumEvent

- (instancetype)initWithTimestamp:(NSDate *)timestamp
                           artist:(NSString *)artist
                            album:(NSString *)album
                         fileType:(NSString *)fileType
                       sourceType:(BCWSourceType)sourceType
                        eventType:(BCWEventType)eventType
                       sourcePath:(NSString *)sourcePath
                         destPath:(NSString *)destPath {
    self = [super init];
    if (self) {
        _timestamp = timestamp;
        _artist = [artist copy];
        _album = [album copy];
        _fileType = [fileType copy];
        _sourceType = sourceType;
        _eventType = eventType;
        _sourcePath = [sourcePath copy];
        _destPath = [destPath copy];
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %@ - %@ (%@)>",
            NSStringFromClass([self class]),
            self.artist, self.album, self.fileType];
}

@end

@implementation BCWRuntimeStatus

- (instancetype)initWithPID:(NSInteger)pid
                       mode:(NSString *)mode
                     status:(NSString *)status
                  startedAt:(NSDate *)startedAt
                heartbeatAt:(nullable NSDate *)heartbeatAt
                 lastScanAt:(nullable NSDate *)lastScanAt
                  lastError:(nullable NSString *)lastError {
    self = [super init];
    if (self) {
        _pid = pid;
        _mode = [mode copy];
        _status = [status copy];
        _startedAt = startedAt;
        _heartbeatAt = heartbeatAt;
        _lastScanAt = lastScanAt;
        _lastError = [lastError copy];
    }
    return self;
}

- (BOOL)isStale {
    return [self secondsSinceHeartbeat] > kStaleThreshold;
}

- (NSTimeInterval)secondsSinceHeartbeat {
    if (!self.heartbeatAt) return DBL_MAX;
    return [[NSDate date] timeIntervalSinceDate:self.heartbeatAt];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %@ (pid: %ld, heartbeat: %@ ago)>",
            NSStringFromClass([self class]),
            self.status, (long)self.pid,
            self.isStale ? @"stale" : [NSString stringWithFormat:@"%.0fs", [self secondsSinceHeartbeat]]];
}

@end

@interface BCWStateStore () {
    sqlite3 *_db;
}
@property (readwrite, nonatomic, copy) NSString *dbPath;
@property (readwrite, nonatomic, getter=isAvailable) BOOL available;
@end

@implementation BCWStateStore

- (instancetype)init {
    return [self initWithDBPath:nil];
}

- (instancetype)initWithDBPath:(nullable NSString *)path {
    self = [super init];
    if (self) {
        if (path) {
            _dbPath = [path copy];
        } else {
            _dbPath = [self defaultDBPath];
        }
        _available = NO;
        [self refresh];
    }
    return self;
}

- (NSString *)defaultDBPath {
    // Try XDG_STATE_HOME first, then ~/.local/state, then fallback
    NSString *home = NSHomeDirectory();
    
    // Check XDG_STATE_HOME environment
    const char *xdgState = getenv("XDG_STATE_HOME");
    if (xdgState) {
        return [[NSString stringWithUTF8String:xdgState]
                stringByAppendingPathComponent:@"bandcamp_watcher/state.sqlite3"];
    }
    
    // Fallback to ~/.local/state
    return [home stringByAppendingPathComponent:kXDGStateRelativePath];
}

- (void)refresh {
    // Close existing connection
    if (_db) {
        sqlite3_close(_db);
        _db = NULL;
    }
    
    // Check if file exists
    NSFileManager *fm = [NSFileManager defaultManager];
    self.available = [fm fileExistsAtPath:self.dbPath];
    
    if (!self.available) {
        return;
    }
    
    // Try to open database
    int rc = sqlite3_open(self.dbPath.UTF8String, &_db);
    if (rc != SQLITE_OK) {
        self.available = NO;
        if (_db) {
            sqlite3_close(_db);
            _db = NULL;
        }
    }
}

- (void)dealloc {
    if (_db) {
        sqlite3_close(_db);
    }
}

#pragma mark - Queries

- (nullable BCWRuntimeStatus *)fetchRuntimeStatus {
    if (!_db) {
        [self refresh];
        if (!_db) return nil;
    }
    
    const char *sql = "SELECT pid, mode, status, started_at, heartbeat_at, last_error, last_scan_at "
                      "FROM runtime WHERE id = 1;";
    
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return nil;
    }
    
    BCWRuntimeStatus *status = nil;
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        NSInteger pid = sqlite3_column_int(stmt, 0);
        
        // mode is integer enum, map to string
        int modeVal = sqlite3_column_int(stmt, 1);
        NSString *mode = (modeVal == 2) ? @"oneshot" : @"watch";
        
        // status is integer enum, map to string
        int statusVal = sqlite3_column_int(stmt, 2);
        NSString *statusStr = [self statusStringFromEnum:statusVal];
        
        // timestamps
        NSDate *startedAt = [self dateFromColumn:stmt index:3];
        NSDate *heartbeatAt = [self dateFromColumn:stmt index:4];
        
        const char *errorCStr = (const char *)sqlite3_column_text(stmt, 5);
        NSString *lastError = errorCStr ? [[NSString alloc] initWithUTF8String:errorCStr] : nil;
        
        NSDate *lastScanAt = [self dateFromColumn:stmt index:6];
        
        status = [[BCWRuntimeStatus alloc] initWithPID:pid
                                                   mode:mode
                                                 status:statusStr
                                              startedAt:startedAt
                                            heartbeatAt:heartbeatAt
                                             lastScanAt:lastScanAt
                                              lastError:lastError];
    }
    
    sqlite3_finalize(stmt);
    return status;
}

- (NSArray<BCWAlbumEvent *> *)fetchRecentAlbums:(NSUInteger)limit {
    // Fetch both album_copied (1) and album_skipped (5) events
    return [self fetchRecentEventsOfTypes:@[@1, @5] limit:limit];  // 1 = COPIED, 5 = SKIPPED
}

- (NSArray<BCWAlbumEvent *> *)fetchRecentEvents:(NSUInteger)limit {
    // Fetch all event types
    return [self fetchRecentEventsOfType:0 limit:limit];  // 0 = any type
}

#pragma mark - Private

- (NSArray<BCWAlbumEvent *> *)fetchRecentEventsOfTypes:(NSArray<NSNumber *> *)types limit:(NSUInteger)limit {
    if (!_db) {
        [self refresh];
        if (!_db) return @[];
    }
    
    // Build IN clause for types
    NSMutableArray *placeholders = [NSMutableArray array];
    for (int i = 0; i < types.count; i++) {
        [placeholders addObject:@"?"];
    }
    NSString *inClause = [placeholders componentsJoinedByString:@","];
    
    NSString *sqlStr = [NSString stringWithFormat:
        @"SELECT ts, type, artist, album, file_type, source_type, src_path, dest_path "
        @"FROM events WHERE type IN (%@) ORDER BY ts DESC LIMIT ?;", inClause];
    
    const char *sql = sqlStr.UTF8String;
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return @[];
    }
    
    // Bind type parameters
    int paramIdx = 1;
    for (NSNumber *typeNum in types) {
        sqlite3_bind_int(stmt, paramIdx++, typeNum.intValue);
    }
    sqlite3_bind_int64(stmt, paramIdx, (sqlite3_int64)limit);
    
    NSMutableArray *events = [NSMutableArray array];
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        NSDate *ts = [self dateFromColumn:stmt index:0];
        
        int eventTypeVal = sqlite3_column_int(stmt, 1);
        BCWEventType eventType = (BCWEventType)eventTypeVal;
        
        const char *artistCStr = (const char *)sqlite3_column_text(stmt, 2);
        NSString *artist = artistCStr ? [[NSString alloc] initWithUTF8String:artistCStr] : @"";
        
        const char *albumCStr = (const char *)sqlite3_column_text(stmt, 3);
        NSString *album = albumCStr ? [[NSString alloc] initWithUTF8String:albumCStr] : @"";
        
        const char *fileTypeCStr = (const char *)sqlite3_column_text(stmt, 4);
        NSString *fileType = fileTypeCStr ? [[NSString alloc] initWithUTF8String:fileTypeCStr] : @"";
        
        int sourceTypeVal = sqlite3_column_int(stmt, 5);
        BCWSourceType sourceType = (sourceTypeVal == 2) ? BCWSourceTypeQobuz : BCWSourceTypeBandcamp;
        
        const char *srcPathCStr = (const char *)sqlite3_column_text(stmt, 6);
        NSString *srcPath = srcPathCStr ? [[NSString alloc] initWithUTF8String:srcPathCStr] : @"";
        
        const char *destPathCStr = (const char *)sqlite3_column_text(stmt, 7);
        NSString *destPath = destPathCStr ? [[NSString alloc] initWithUTF8String:destPathCStr] : @"";
        
        BCWAlbumEvent *event = [[BCWAlbumEvent alloc] initWithTimestamp:ts
                                                                  artist:artist
                                                                   album:album
                                                                fileType:fileType
                                                              sourceType:sourceType
                                                               eventType:eventType
                                                              sourcePath:srcPath
                                                                destPath:destPath];
        [events addObject:event];
    }
    
    sqlite3_finalize(stmt);
    return [events copy];
}

- (NSArray<BCWAlbumEvent *> *)fetchRecentEventsOfType:(int)type limit:(NSUInteger)limit {
    if (!_db) {
        [self refresh];
        if (!_db) return @[];
    }
    
    const char *sql;
    if (type > 0) {
        sql = "SELECT ts, type, artist, album, file_type, source_type, src_path, dest_path "
              "FROM events WHERE type = ? ORDER BY ts DESC LIMIT ?;";
    } else {
        sql = "SELECT ts, type, artist, album, file_type, source_type, src_path, dest_path "
              "FROM events ORDER BY ts DESC LIMIT ?;";
    }
    
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return @[];
    }
    
    int paramIdx = 1;
    if (type > 0) {
        sqlite3_bind_int(stmt, paramIdx++, type);
    }
    sqlite3_bind_int64(stmt, paramIdx, (sqlite3_int64)limit);
    
    NSMutableArray *events = [NSMutableArray array];
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        NSDate *ts = [self dateFromColumn:stmt index:0];
        
        int eventTypeVal = sqlite3_column_int(stmt, 1);
        BCWEventType eventType = (BCWEventType)eventTypeVal;
        
        const char *artistCStr = (const char *)sqlite3_column_text(stmt, 2);
        NSString *artist = artistCStr ? [[NSString alloc] initWithUTF8String:artistCStr] : @"";
        
        const char *albumCStr = (const char *)sqlite3_column_text(stmt, 3);
        NSString *album = albumCStr ? [[NSString alloc] initWithUTF8String:albumCStr] : @"";
        
        const char *fileTypeCStr = (const char *)sqlite3_column_text(stmt, 4);
        NSString *fileType = fileTypeCStr ? [[NSString alloc] initWithUTF8String:fileTypeCStr] : @"";
        
        int sourceTypeVal = sqlite3_column_int(stmt, 5);
        BCWSourceType sourceType = (sourceTypeVal == 2) ? BCWSourceTypeQobuz : BCWSourceTypeBandcamp;
        
        const char *srcPathCStr = (const char *)sqlite3_column_text(stmt, 6);
        NSString *srcPath = srcPathCStr ? [[NSString alloc] initWithUTF8String:srcPathCStr] : @"";
        
        const char *destPathCStr = (const char *)sqlite3_column_text(stmt, 7);
        NSString *destPath = destPathCStr ? [[NSString alloc] initWithUTF8String:destPathCStr] : @"";
        
        BCWAlbumEvent *event = [[BCWAlbumEvent alloc] initWithTimestamp:ts
                                                                  artist:artist
                                                                   album:album
                                                                fileType:fileType
                                                              sourceType:sourceType
                                                               eventType:eventType
                                                              sourcePath:srcPath
                                                                destPath:destPath];
        [events addObject:event];
    }
    
    sqlite3_finalize(stmt);
    return [events copy];
}

- (nullable NSDate *)dateFromColumn:(sqlite3_stmt *)stmt index:(int)index {
    sqlite3_int64 val = sqlite3_column_int64(stmt, index);
    if (val == 0) return nil;
    return [NSDate dateWithTimeIntervalSince1970:(NSTimeInterval)val];
}

- (NSString *)statusStringFromEnum:(int)status {
    switch (status) {
        case 1: return @"starting";
        case 2: return @"running";
        case 3: return @"idle";
        case 4: return @"stopping";
        case 5: return @"stopped";
        case 6: return @"error";
        default: return @"unknown";
    }
}

@end
