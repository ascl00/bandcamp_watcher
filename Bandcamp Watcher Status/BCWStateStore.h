//
//  BCWStateStore.h
//  Bandcamp Watcher Status
//
//  Read-only access to daemon's SQLite state database
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, BCWEventType) {
    BCWEventTypeUnknown = 0,
    BCWEventTypeWatcherStarted = 1,
    BCWEventTypeWatcherStopped = 2,
    BCWEventTypeWatcherError = 3,
    BCWEventTypeAlbumCopied = 4,
    BCWEventTypeAlbumSkipped = 5,
    BCWEventTypeCopyFailed = 6,
    BCWEventTypeOneshotCompleted = 7
};

typedef NS_ENUM(NSInteger, BCWSourceType) {
    BCWSourceTypeUnknown = 0,
    BCWSourceTypeBandcamp = 1,
    BCWSourceTypeQobuz = 2
};

@interface BCWAlbumEvent : NSObject
@property (readonly, nonatomic) NSDate *timestamp;
@property (readonly, nonatomic, copy) NSString *artist;
@property (readonly, nonatomic, copy) NSString *album;
@property (readonly, nonatomic, copy) NSString *fileType;
@property (readonly, nonatomic) BCWSourceType sourceType;
@property (readonly, nonatomic) BCWEventType eventType;
@property (readonly, nonatomic, copy) NSString *sourcePath;
@property (readonly, nonatomic, copy) NSString *destPath;
@end

@interface BCWRuntimeStatus : NSObject
@property (readonly, nonatomic) NSInteger pid;
@property (readonly, nonatomic, copy) NSString *mode;  // "watch" or "oneshot"
@property (readonly, nonatomic, copy) NSString *status; // "starting", "running", etc.
@property (readonly, nonatomic) NSDate *startedAt;
@property (readonly, nonatomic, nullable) NSDate *heartbeatAt;
@property (readonly, nonatomic, nullable) NSDate *lastScanAt;
@property (readonly, nonatomic, copy, nullable) NSString *lastError;

- (BOOL)isStale;  // No heartbeat for 60+ seconds
- (NSTimeInterval)secondsSinceHeartbeat;
@end

@interface BCWStateStore : NSObject

@property (readonly, nonatomic, copy) NSString *dbPath;
@property (readonly, nonatomic, getter=isAvailable) BOOL available;

- (instancetype)init;
- (instancetype)initWithDBPath:(nullable NSString *)path;

// Runtime status (single row, id=1)
- (nullable BCWRuntimeStatus *)fetchRuntimeStatus;

// Recent album events (album_copied type)
- (NSArray<BCWAlbumEvent *> *)fetchRecentAlbums:(NSUInteger)limit;

// All event types (for debugging)
- (NSArray<BCWAlbumEvent *> *)fetchRecentEvents:(NSUInteger)limit;

// Convenience
- (void)refresh;  // Re-check database availability

@end

NS_ASSUME_NONNULL_END
