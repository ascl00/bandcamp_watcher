//
//  UtilsFileTests.m
//  bandcamp_watcherTests
//
//  Tests for new file utility functions in utils.c
//

#import <XCTest/XCTest.h>

#include "utils.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

@interface UtilsFileTests : XCTestCase {
    char testDir[512];
}
@end

@implementation UtilsFileTests

- (void)setUp {
    [super setUp];
    strcpy(self->testDir, "/tmp/bcw_utils_test_XXXXXX");
    XCTAssertTrue(mkdtemp(self->testDir) != NULL, @"Failed to create test directory");
}

- (void)tearDown {
    NSString *cmd = [NSString stringWithFormat:@"rm -rf %s", self->testDir];
    system([cmd UTF8String]);
    [super tearDown];
}

#pragma mark - files_with_extensions tests

- (void)testFilesWithExtensionsSingleMatch {
    const char *exts[] = {"flac"};
    
    // Create a flac file
    char path[512];
    snprintf(path, sizeof(path), "%s/song.flac", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    size_t count = files_with_extensions(self->testDir, exts, 1);
    XCTAssertEqual(count, 1);
}

- (void)testFilesWithExtensionsMultipleTypes {
    const char *exts[] = {"flac", "m4a"};
    
    // Create files of different types
    char path[512];
    snprintf(path, sizeof(path), "%s/song1.flac", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    snprintf(path, sizeof(path), "%s/song2.m4a", self->testDir);
    fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    snprintf(path, sizeof(path), "%s/song3.mp3", self->testDir);
    fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    // Should count flac and m4a, but not mp3
    size_t count = files_with_extensions(self->testDir, exts, 2);
    XCTAssertEqual(count, 2);
}

- (void)testFilesWithExtensionsEmptyDir {
    const char *exts[] = {"flac"};
    
    size_t count = files_with_extensions(self->testDir, exts, 1);
    XCTAssertEqual(count, 0);
}

- (void)testFilesWithExtensionsNoMatch {
    const char *exts[] = {"flac"};
    
    // Create only mp3 files
    char path[512];
    snprintf(path, sizeof(path), "%s/song.mp3", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    size_t count = files_with_extensions(self->testDir, exts, 1);
    XCTAssertEqual(count, 0);
}

@end
