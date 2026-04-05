//
//  FileOpsTests.m
//  bandcamp_watcherTests
//
//  Tests for copy.c functions
//

#import <XCTest/XCTest.h>

#include "copy.h"
#include <sys/stat.h>
#include <unistd.h>

@interface FileOpsTests : XCTestCase {
    char testDir[512];
}
@end

@implementation FileOpsTests

- (void)setUp {
    [super setUp];
    strcpy(self->testDir, "/tmp/bcw_fileops_test_XXXXXX");
    XCTAssertTrue(mkdtemp(self->testDir) != NULL, @"Failed to create test directory");
}

- (void)tearDown {
    NSString *cmd = [NSString stringWithFormat:@"rm -rf %s", self->testDir];
    system([cmd UTF8String]);
    [super tearDown];
}

#pragma mark - dir_exists tests

- (void)testDirExistsReturnsTrueForExistingDir {
    XCTAssertTrue(dir_exists(self->testDir));
}

- (void)testDirExistsReturnsFalseForNonexistentDir {
    XCTAssertFalse(dir_exists("/tmp/nonexistent_bcw_test_12345"));
}

- (void)testDirExistsReturnsFalseForFile {
    char path[512];
    snprintf(path, sizeof(path), "%s/testfile.txt", self->testDir);
    
    FILE *f = fopen(path, "w");
    fprintf(f, "test");
    fclose(f);
    
    // File exists but is not a directory
    XCTAssertFalse(dir_exists(path));
}

#pragma mark - copy tests

- (void)testCopyCreatesDestinationFile {
    char src[512], dst[512];
    snprintf(src, sizeof(src), "%s/source.txt", self->testDir);
    snprintf(dst, sizeof(dst), "%s/dest.txt", self->testDir);
    
    FILE *f = fopen(src, "w");
    fprintf(f, "Hello, World!");
    fclose(f);
    
    XCTAssertEqual(copy(src, dst), 0);
    
    FILE *check = fopen(dst, "r");
    XCTAssertTrue(check != NULL);
    fclose(check);
}

- (void)testCopyCopiesContentCorrectly {
    char src[512], dst[512];
    snprintf(src, sizeof(src), "%s/source.txt", self->testDir);
    snprintf(dst, sizeof(dst), "%s/dest.txt", self->testDir);
    
    const char *content = "This is a test file with some content.";
    FILE *f = fopen(src, "w");
    fprintf(f, "%s", content);
    fclose(f);
    
    XCTAssertEqual(copy(src, dst), 0);
    
    FILE *check = fopen(dst, "r");
    char buf[256];
    fgets(buf, sizeof(buf), check);
    fclose(check);
    
    XCTAssertEqualObjects(@(buf), @(content));
}

- (void)testCopyFailsForNonexistentSource {
    char src[512], dst[512];
    snprintf(src, sizeof(src), "%s/nonexistent.txt", self->testDir);
    snprintf(dst, sizeof(dst), "%s/dest.txt", self->testDir);
    
    int result = copy(src, dst);
    XCTAssertNotEqual(result, 0);
}

- (void)testCopyCopiesBinaryContent {
    char src[512], dst[512];
    snprintf(src, sizeof(src), "%s/binary.bin", self->testDir);
    snprintf(dst, sizeof(dst), "%s/binary_copy.bin", self->testDir);
    
    // Create binary file with null bytes
    FILE *f = fopen(src, "wb");
    unsigned char data[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0x00};
    fwrite(data, 1, sizeof(data), f);
    fclose(f);
    
    XCTAssertEqual(copy(src, dst), 0);
    
    // Verify content
    FILE *check = fopen(dst, "rb");
    unsigned char buf[6];
    size_t n = fread(buf, 1, sizeof(buf), check);
    fclose(check);
    
    XCTAssertEqual(n, 6);
    XCTAssertEqual(memcmp(data, buf, 6), 0);
}

#pragma mark - clone tests

- (void)testCloneCreatesDestinationDir {
    char dst[512];
    snprintf(dst, sizeof(dst), "%s_clone", self->testDir);
    
    XCTAssertFalse(dir_exists(dst));
    XCTAssertEqual(clone(self->testDir, dst), 0);
    XCTAssertTrue(dir_exists(dst));
}

- (void)testCloneFailsForNonexistentSource {
    char src[] = "/tmp/nonexistent_bcw_source";
    char dst[] = "/tmp/nonexistent_bcw_dest";
    
    XCTAssertEqual(clone(src, dst), ENOENT);
}

- (void)testCloneCopiesFiles {
    char dst[512], src_file[512], dst_file[512];
    snprintf(dst, sizeof(dst), "%s_clone", self->testDir);
    snprintf(src_file, sizeof(src_file), "%s/file.txt", self->testDir);
    
    // Create a file in source
    FILE *f = fopen(src_file, "w");
    fprintf(f, "test content");
    fclose(f);
    
    XCTAssertEqual(clone(self->testDir, dst), 0);
    
    // Check file was copied
    snprintf(dst_file, sizeof(dst_file), "%s/file.txt", dst);
    FILE *check = fopen(dst_file, "r");
    XCTAssertTrue(check != NULL);
    fclose(check);
}

- (void)testCloneCopiesMultipleFiles {
    char dst[512];
    snprintf(dst, sizeof(dst), "%s_clone", self->testDir);
    
    // Create multiple files
    for (int i = 0; i < 3; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/file%d.txt", self->testDir, i);
        FILE *f = fopen(path, "w");
        fprintf(f, "content %d", i);
        fclose(f);
    }
    
    XCTAssertEqual(clone(self->testDir, dst), 0);
    
    // Check all files exist
    for (int i = 0; i < 3; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/file%d.txt", dst, i);
        FILE *check = fopen(path, "r");
        XCTAssertTrue(check != NULL);
        fclose(check);
    }
}

@end
