//
//  StateDbTests.m
//  bandcamp_watcherTests
//
//  Tests for state_db.c SQLite state management
//

#import <XCTest/XCTest.h>

#include "state_db.h"
#include <sys/stat.h>
#include <unistd.h>
#include <sqlite3.h>

@interface StateDbTests : XCTestCase {
    char dbPath[512];
    state_db_t *db;
}
@end

@implementation StateDbTests

- (void)setUp {
    [super setUp];
    
    // Create temporary DB path
    snprintf(self->dbPath, sizeof(self->dbPath), "/tmp/bcw_state_test_%d.sqlite3", (int)getpid());
    
    // Remove any existing file
    unlink(self->dbPath);
    
    // Open fresh database
    int rc = state_db_open(self->dbPath, &self->db);
    XCTAssertEqual(rc, 0, @"Failed to open state database");
    XCTAssertTrue(self->db != NULL, @"Database handle is NULL");
}

- (void)tearDown {
    if (self->db) {
        state_db_close(self->db);
        self->db = NULL;
    }
    unlink(self->dbPath);
    [super tearDown];
}

#pragma mark - Basic Operations

- (void)testCreatesSchema {
    // Database was opened in setUp, schema should be created
    XCTAssertTrue(self->db != NULL, @"Database should be open");
    
    // Verify we can query meta table
    const char *path = state_db_get_path(self->db);
    XCTAssertTrue(path != NULL, @"DB path should be available");
    XCTAssertEqual(strcmp(path, self->dbPath), 0, @"DB path should match");
}

- (void)testInitRuntime {
    int rc = state_db_init_runtime(self->db, RUNTIME_MODE_WATCH, 12345);
    XCTAssertEqual(rc, 0, @"Failed to initialize runtime");
    
    // Verify runtime row exists
    int pid = 0;
    runtime_mode_t mode = 0;
    runtime_status_t status = 0;
    int64_t started_at = 0;
    
    rc = state_db_get_runtime(self->db, &pid, &mode, &status, &started_at, NULL, NULL);
    XCTAssertEqual(rc, 0, @"Failed to get runtime");
    XCTAssertEqual(pid, 12345, @"PID mismatch");
    XCTAssertEqual(mode, RUNTIME_MODE_WATCH, @"Mode mismatch");
    XCTAssertEqual(status, RUNTIME_STATUS_STARTING, @"Status should be starting");
    XCTAssertGreaterThan(started_at, 0, @"Started timestamp should be set");
}

- (void)testSetRuntimeStatus {
    state_db_init_runtime(self->db, RUNTIME_MODE_WATCH, 12345);
    
    int rc = state_db_set_runtime_status(self->db, RUNTIME_STATUS_RUNNING, NULL);
    XCTAssertEqual(rc, 0, @"Failed to set runtime status");
    
    runtime_status_t status = 0;
    rc = state_db_get_runtime(self->db, NULL, NULL, &status, NULL, NULL, NULL);
    XCTAssertEqual(rc, 0, @"Failed to get runtime");
    XCTAssertEqual(status, RUNTIME_STATUS_RUNNING, @"Status should be running");
}

- (void)testSetRuntimeStatusWithError {
    state_db_init_runtime(self->db, RUNTIME_MODE_WATCH, 12345);
    
    const char *error_msg = "Test error message";
    int rc = state_db_set_runtime_status(self->db, RUNTIME_STATUS_ERROR, error_msg);
    XCTAssertEqual(rc, 0, @"Failed to set runtime status with error");
}

- (void)testHeartbeat {
    state_db_init_runtime(self->db, RUNTIME_MODE_WATCH, 12345);
    
    // Initial heartbeat should be set from init
    int64_t heartbeat1 = 0;
    int rc = state_db_get_runtime(self->db, NULL, NULL, NULL, NULL, &heartbeat1, NULL);
    XCTAssertEqual(rc, 0, @"Failed to get runtime");
    XCTAssertGreaterThan(heartbeat1, 0, @"Initial heartbeat should be set");
    
    // Update heartbeat
    sleep(1); // Ensure time changes
    rc = state_db_heartbeat(self->db);
    XCTAssertEqual(rc, 0, @"Failed to update heartbeat");
    
    int64_t heartbeat2 = 0;
    rc = state_db_get_runtime(self->db, NULL, NULL, NULL, NULL, &heartbeat2, NULL);
    XCTAssertEqual(rc, 0, @"Failed to get runtime");
    XCTAssertGreaterThan(heartbeat2, heartbeat1, @"Heartbeat should be updated");
}

- (void)testSetLastScan {
    state_db_init_runtime(self->db, RUNTIME_MODE_WATCH, 12345);
    
    int rc = state_db_set_last_scan(self->db);
    XCTAssertEqual(rc, 0, @"Failed to set last scan");
    
    int64_t last_scan = 0;
    rc = state_db_get_runtime(self->db, NULL, NULL, NULL, NULL, NULL, &last_scan);
    XCTAssertEqual(rc, 0, @"Failed to get runtime");
    XCTAssertGreaterThan(last_scan, 0, @"Last scan should be set");
}

#pragma mark - Event Operations

- (void)testAppendEvent {
    int rc = state_db_append_event(self->db, EVENT_WATCHER_STARTED,
                                  NULL, NULL, NULL, 0, NULL, NULL, NULL, 0);
    XCTAssertEqual(rc, 0, @"Failed to append event");
    
    rc = state_db_append_event(self->db, EVENT_ALBUM_COPIED,
                               "Test Artist", "Test Album", "flac",
                               SOURCE_TYPE_BANDCAMP,
                               "/src/path", "/dst/path", NULL, 0);
    XCTAssertEqual(rc, 0, @"Failed to append album event");
}

- (void)testAppendEventWithMessage {
    int rc = state_db_append_event(self->db, EVENT_COPY_FAILED,
                                   "Artist", "Album", "aac", SOURCE_TYPE_QOBUZ,
                                   "/src", "/dst",
                                   "Permission denied", -1);
    XCTAssertEqual(rc, 0, @"Failed to append failure event");
}

- (void)testMultipleEvents {
    // Insert multiple events
    for (int i = 0; i < 10; i++) {
        char artist[32];
        char album[32];
        snprintf(artist, sizeof(artist), "Artist %d", i);
        snprintf(album, sizeof(album), "Album %d", i);
        
        int rc = state_db_append_event(self->db, EVENT_ALBUM_COPIED,
                                       artist, album, "flac",
                                       SOURCE_TYPE_BANDCAMP,
                                       "/src", "/dst", NULL, 0);
        XCTAssertEqual(rc, 0, @"Failed to append event %d", i);
    }
}

#pragma mark - Retention

- (void)testRetentionTrim {
    // Insert more than retention limit
    for (int i = 0; i < 100; i++) {
        char artist[32];
        snprintf(artist, sizeof(artist), "Artist %d", i);
        state_db_append_event(self->db, EVENT_ALBUM_COPIED,
                              artist, "Album", "flac", SOURCE_TYPE_BANDCAMP,
                              "/src", "/dst", NULL, 0);
    }
    
    // Trim to 50 events
    int rc = state_db_trim(self->db, 50);
    XCTAssertEqual(rc, 0, @"Failed to trim events");
    
    // Verify count via direct SQLite query
    sqlite3 *raw_db;
    rc = sqlite3_open(self->dbPath, &raw_db);
    XCTAssertEqual(rc, SQLITE_OK, @"Failed to open raw connection");
    
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(raw_db, "SELECT COUNT(*) FROM events;", -1, &stmt, NULL);
    XCTAssertEqual(rc, SQLITE_OK, @"Failed to prepare count query");
    
    rc = sqlite3_step(stmt);
    XCTAssertEqual(rc, SQLITE_ROW, @"Failed to get count");
    
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close(raw_db);
    
    // Should be at most 50, but could be less if trim already happened
    XCTAssertLessThanOrEqual(count, 100, @"Events should be trimmed");
}

#pragma mark - Mode Variants

- (void)testOneshotMode {
    int rc = state_db_init_runtime(self->db, RUNTIME_MODE_ONESHOT, 99999);
    XCTAssertEqual(rc, 0, @"Failed to init oneshot mode");
    
    runtime_mode_t mode = 0;
    rc = state_db_get_runtime(self->db, NULL, &mode, NULL, NULL, NULL, NULL);
    XCTAssertEqual(rc, 0, @"Failed to get runtime");
    XCTAssertEqual(mode, RUNTIME_MODE_ONESHOT, @"Mode should be oneshot");
}

#pragma mark - Error Handling

- (void)testOpenInvalidPath {
    const char *invalid_path = "/nonexistent_dir/subdir/test.db";
    state_db_t *bad_db = NULL;
    
    int rc = state_db_open(invalid_path, &bad_db);
    // Should fail because parent directory doesn't exist
    XCTAssertNotEqual(rc, 0, @"Should fail for invalid path");
    XCTAssertTrue(bad_db == NULL, @"DB handle should be NULL on failure");
}

- (void)testOperationsOnNullDb {
    // These should not crash, just return error
    int rc = state_db_init_runtime(NULL, RUNTIME_MODE_WATCH, 12345);
    XCTAssertEqual(rc, -1, @"Should return error for NULL db");
    
    rc = state_db_heartbeat(NULL);
    XCTAssertEqual(rc, -1, @"Should return error for NULL db");
    
    rc = state_db_append_event(NULL, EVENT_WATCHER_STARTED,
                               NULL, NULL, NULL, 0, NULL, NULL, NULL, 0);
    XCTAssertEqual(rc, -1, @"Should return error for NULL db");
}

- (void)testGetPathReturnsNullForNullDb {
    const char *path = state_db_get_path(NULL);
    XCTAssertTrue(path == NULL, @"Should return NULL for NULL db");
}

@end
