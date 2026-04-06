//
//  ValidateTests.m
//  bandcamp_watcherTests
//
//  Tests for validate.c unified folder validation
//

#import <XCTest/XCTest.h>

#include "folder.h"
#include "bandcamp.h"

@interface ValidateTests : XCTestCase {
    char testDir[512];
}
@end

@implementation ValidateTests

- (void)setUp {
    [super setUp];
    strcpy(self->testDir, "/tmp/bcw_validate_test_XXXXXX");
    XCTAssertTrue(mkdtemp(self->testDir) != NULL, @"Failed to create test directory");
}

- (void)tearDown {
    NSString *cmd = [NSString stringWithFormat:@"rm -rf %s", self->testDir];
    system([cmd UTF8String]);
    [super tearDown];
}

#pragma mark - check_music_folder tests

- (void)testDetectsBandcampFormat {
    const char *exts[] = {"flac"};
    
    // Create Bandcamp-style files: "Artist - Album - NN Song.flac"
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/Artist - Album - %02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int source_type = SOURCE_UNKNOWN;
    int result = check_music_folder(self->testDir, "Artist - Album", &info, exts, 1, &source_type);
    
    XCTAssertEqual(result, 0);
    XCTAssertEqual(source_type, SOURCE_BANDCAMP);
    XCTAssertEqual(strcmp(info.name, "Artist"), 0);
    XCTAssertEqual(strcmp(info.album, "Album"), 0);
    XCTAssertEqual(strcmp(info.file_type, "flac"), 0);
}

- (void)testDetectsQobuzFormat {
    const char *exts[] = {"flac"};
    
    // Create Qobuz-style files: "NN Song.flac"
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int source_type = SOURCE_UNKNOWN;
    int result = check_music_folder(self->testDir, "Artist - Album", &info, exts, 1, &source_type);
    
    XCTAssertEqual(result, 0);
    XCTAssertEqual(source_type, SOURCE_QOBUZ);
    XCTAssertEqual(strcmp(info.name, "Artist"), 0);
    XCTAssertEqual(strcmp(info.album, "Album"), 0);
    XCTAssertEqual(strcmp(info.file_type, "flac"), 0);
}

- (void)testBandcampTakesPrecedenceOverQobuz {
    const char *exts[] = {"flac"};
    
    // Create BOTH formats - Bandcamp should win
    char path[512];
    
    // Bandcamp-style file
    snprintf(path, sizeof(path), "%s/Artist - Album - 01 Song.flac", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    // Qobuz-style files
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.flac", self->testDir, i);
        fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int source_type = SOURCE_UNKNOWN;
    int result = check_music_folder(self->testDir, "Artist - Album", &info, exts, 1, &source_type);
    
    XCTAssertEqual(result, 0);
    // Should detect as Bandcamp since it's checked first
    XCTAssertEqual(source_type, SOURCE_BANDCAMP);
}

- (void)testInvalidFolderName {
    const char *exts[] = {"flac"};
    
    // Create valid files but with invalid folder name
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int source_type = SOURCE_UNKNOWN;
    int result = check_music_folder(self->testDir, "NoDashInFolderName", &info, exts, 1, &source_type);
    
    XCTAssertEqual(result, -1);
    XCTAssertEqual(source_type, SOURCE_UNKNOWN);
}

- (void)testNoMatchingFiles {
    const char *exts[] = {"flac"};
    
    // Create mp3 files when we're looking for flac
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.mp3", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int source_type = SOURCE_UNKNOWN;
    int result = check_music_folder(self->testDir, "Artist - Album", &info, exts, 1, &source_type);
    
    XCTAssertEqual(result, -1);
}

- (void)testStripsSuffixFromAlbum {
    const char *exts[] = {"flac"};
    
    // Create Qobuz-style files
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int source_type = SOURCE_UNKNOWN;
    // Folder has "-2" suffix
    int result = check_music_folder(self->testDir, "Artist - Album-2", &info, exts, 1, &source_type);
    
    XCTAssertEqual(result, 0);
    XCTAssertEqual(strcmp(info.name, "Artist"), 0);
    XCTAssertEqual(strcmp(info.album, "Album"), 0);  // Should have "-2" stripped
}

- (void)testStripsPreOrderTag {
    const char *exts[] = {"flac"};
    
    // Create Qobuz-style files
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int source_type = SOURCE_UNKNOWN;
    // Folder has "(pre-order)" tag
    int result = check_music_folder(self->testDir, "Artist - Album (pre-order)", &info, exts, 1, &source_type);
    
    XCTAssertEqual(result, 0);
    XCTAssertEqual(strcmp(info.name, "Artist"), 0);
    XCTAssertEqual(strcmp(info.album, "Album"), 0);  // Should have "(pre-order)" stripped
}

- (void)testNullParameters {
    const char *exts[] = {"flac"};
    band_info_t info = {};
    int source_type = SOURCE_UNKNOWN;
    
    XCTAssertEqual(check_music_folder(NULL, "Artist - Album", &info, exts, 1, &source_type), -1);
    XCTAssertEqual(check_music_folder(self->testDir, NULL, &info, exts, 1, &source_type), -1);
    XCTAssertEqual(check_music_folder(self->testDir, "Artist - Album", NULL, exts, 1, &source_type), -1);
    XCTAssertEqual(check_music_folder(self->testDir, "Artist - Album", &info, NULL, 1, &source_type), -1);
    XCTAssertEqual(check_music_folder(self->testDir, "Artist - Album", &info, exts, 1, NULL), -1);
}

- (void)testMultipleExtensionsDetectsCorrectType {
    const char *exts[] = {"flac", "m4a"};
    
    // Create m4a files
    char path[512];
    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%s/%02d Song.m4a", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {};
    int source_type = SOURCE_UNKNOWN;
    int result = check_music_folder(self->testDir, "Artist - Album", &info, exts, 2, &source_type);
    
    XCTAssertEqual(result, 0);
    XCTAssertEqual(source_type, SOURCE_QOBUZ);
    XCTAssertEqual(strcmp(info.file_type, "m4a"), 0);
}

@end
