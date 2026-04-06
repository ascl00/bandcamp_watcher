//
//  QobuzTests.m
//  bandcamp_watcherTests
//
//  Tests for qobuz.c functions
//

#import <XCTest/XCTest.h>

#include "qobuz.h"
#include "bandcamp.h"

@interface QobuzTests : XCTestCase {
    char testDir[512];
}
@end

@implementation QobuzTests

- (void)setUp {
    [super setUp];
    strcpy(self->testDir, "/tmp/bcw_qobuz_test_XXXXXX");
    XCTAssertTrue(mkdtemp(self->testDir) != NULL, @"Failed to create test directory");
}

- (void)tearDown {
    NSString *cmd = [NSString stringWithFormat:@"rm -rf %s", self->testDir];
    system([cmd UTF8String]);
    [super tearDown];
}

#pragma mark - check_qobuz_files tests

- (void)testQobuzValidSequentialTracks {
    const char *exts[] = {"flac"};
    
    // Create 5 sequential track files
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song Name.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        XCTAssertTrue(fd >= 0, @"Failed to create file %d", i);
        close(fd);
    }
    
    band_info_t info = {};
    int result = check_qobuz_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, 0);
    XCTAssertEqual(strcmp(info.file_type, "flac"), 0);
}

- (void)testQobuzTooFewTracks {
    const char *exts[] = {"flac"};
    
    // Create only 2 tracks (minimum is 3)
    char path[512];
    for (int i = 1; i <= 2; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int result = check_qobuz_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, -1);
}

- (void)testQobuzTooManyTracks {
    const char *exts[] = {"flac"};
    
    // Create 31 tracks (maximum is 30)
    char path[512];
    for (int i = 1; i <= 31; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int result = check_qobuz_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, -1);
}

- (void)testQobuzGapInSequence {
    const char *exts[] = {"flac"};
    
    // Create tracks 01, 02, 04 (missing 03)
    char path[512];
    snprintf(path, sizeof(path), "%s/01 Song.flac", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    snprintf(path, sizeof(path), "%s/02 Song.flac", self->testDir);
    fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    snprintf(path, sizeof(path), "%s/04 Song.flac", self->testDir);
    fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    band_info_t info = {};
    int result = check_qobuz_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, -1);
}

- (void)testQobuzTooManyNonAudioFiles {
    const char *exts[] = {"flac"};
    
    // Create 5 tracks
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    // Create 7 non-audio files (max is 6)
    for (int i = 1; i <= 7; i++) {
        snprintf(path, sizeof(path), "%s/extra%d.jpg", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int result = check_qobuz_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, -1);
}

- (void)testQobuzMixedAudioTypes {
    const char *exts[] = {"flac", "m4a"};
    
    // Create tracks with mixed extensions
    char path[512];
    for (int i = 1; i <= 3; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    // Add an m4a track
    snprintf(path, sizeof(path), "%s/04 Song.m4a", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    band_info_t info = {};
    int result = check_qobuz_files(self->testDir, &info, exts, 2);
    XCTAssertEqual(result, -1);
}

- (void)testQobuzValidWithSomeNonAudio {
    const char *exts[] = {"flac"};
    
    // Create 5 tracks
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    // Create 5 non-audio files (within limit of 6)
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/cover%d.jpg", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int result = check_qobuz_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, 0);
}

- (void)testQobuzDetectsM4A {
    const char *exts[] = {"m4a"};
    
    // Create m4a files
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.m4a", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int result = check_qobuz_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, 0);
    XCTAssertEqual(strcmp(info.file_type, "m4a"), 0);
}

- (void)testQobuzNoAudioFiles {
    const char *exts[] = {"flac"};
    
    // Create only non-audio files
    char path[512];
    snprintf(path, sizeof(path), "%s/cover.jpg", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    band_info_t info = {};
    int result = check_qobuz_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, -1);
}

- (void)testQobuzNonSequentialNumbers {
    const char *exts[] = {"flac"};
    
    // Create files that aren't sequential track numbers
    char path[512];
    snprintf(path, sizeof(path), "%s/10 Song.flac", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    snprintf(path, sizeof(path), "%s/20 Song.flac", self->testDir);
    fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    snprintf(path, sizeof(path), "%s/30 Song.flac", self->testDir);
    fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    band_info_t info = {};
    int result = check_qobuz_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, -1);  // Should fail - doesn't start at 01
}

@end
