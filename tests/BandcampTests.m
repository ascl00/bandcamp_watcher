//
//  BandcampTests.m
//  bandcamp_watcherTests
//
//  Tests for bandcamp.c functions
//

#import <XCTest/XCTest.h>

#include "bandcamp.h"
#include "utils.h"
#include "folder.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

@interface BandcampTests : XCTestCase {
    char testDir[512];
}
@end

@implementation BandcampTests

- (void)setUp {
    [super setUp];
    strcpy(self->testDir, "/tmp/bcw_bandcamp_test_XXXXXX");
    XCTAssertTrue(mkdtemp(self->testDir) != NULL, @"Failed to create test directory");
}

- (void)tearDown {
    NSString *cmd = [NSString stringWithFormat:@"rm -rf %s", self->testDir];
    system([cmd UTF8String]);
    [super tearDown];
}

#pragma mark - files_with_extension tests (now in utils)

- (void)testFilesWithExtensionEmptyDir {
    size_t count = files_with_extension(self->testDir, ".flac");
    XCTAssertEqual(count, 0);
}

- (void)testFilesWithExtensionCountsMatchingFiles {
    // Create test files
    for (int i = 0; i < 3; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/song%d.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    // Create non-matching file
    char path[512];
    snprintf(path, sizeof(path), "%s/song.txt", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    size_t count = files_with_extension(self->testDir, ".flac");
    XCTAssertEqual(count, 3);
}

- (void)testFilesWithExtensionCaseInsensitive {
    char path[512];
    snprintf(path, sizeof(path), "%s/song.FLAC", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    size_t count = files_with_extension(self->testDir, ".flac");
    XCTAssertEqual(count, 1);
}

#pragma mark - band_info_string tests

- (void)testBandInfoStringFlac {
    band_info_t info = {
        .name = "Test Band",
        .album = "Test Album",
        .file_type = "flac"
    };
    const char *result = band_info_string(&info);
    XCTAssertTrue(strstr(result, "Test Band") != NULL);
    XCTAssertTrue(strstr(result, "Test Album") != NULL);
    XCTAssertTrue(strstr(result, "flac") != NULL);
}

- (void)testBandInfoStringAac {
    band_info_t info = {
        .name = "Artist",
        .album = "Album",
        .file_type = "aac"
    };
    const char *result = band_info_string(&info);
    XCTAssertTrue(strstr(result, "aac") != NULL);
}

- (void)testBandInfoStringM4a {
    band_info_t info = {
        .name = "Artist",
        .album = "Album",
        .file_type = "m4a"
    };
    const char *result = band_info_string(&info);
    XCTAssertTrue(strstr(result, "m4a") != NULL);
}

#pragma mark - files_that_look_like_songs tests

- (void)testFilesThatLookLikeSongsBasic {
    // Create Bandcamp-style named files
    char path[512];
    for (int i = 1; i <= 3; i++) {
        snprintf(path, sizeof(path), "%s/Band - Album - %02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    size_t count = files_that_look_like_songs(self->testDir, "Band", "Album", "flac");
    XCTAssertEqual(count, 3);
}

- (void)testFilesThatLookLikeSongsWrongName {
    // Create files with wrong band/album prefix
    char path[512];
    for (int i = 1; i <= 3; i++) {
        snprintf(path, sizeof(path), "%s/WrongBand - WrongAlbum - %02d Song.flac", self->testDir, i);
        int fd = open(path, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    size_t count = files_that_look_like_songs(self->testDir, "Band", "Album", "flac");
    XCTAssertEqual(count, 0);
}

- (void)testFilesThatLookLikeSongsMixedTypes {
    // Create files with different extensions
    char path[512];
    snprintf(path, sizeof(path), "%s/Band - Album - 01 Song.flac", self->testDir);
    int fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    snprintf(path, sizeof(path), "%s/Band - Album - 02 Song.m4a", self->testDir);
    fd = open(path, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    // Should only count flac files when asked for flac
    size_t count = files_that_look_like_songs(self->testDir, "Band", "Album", "flac");
    XCTAssertEqual(count, 1);
}

#pragma mark - check_bandcamp_files tests

- (void)testCheckBandcampFilesNoAudioFiles {
    const char *exts[] = {"flac"};
    band_info_t info = {
        .name = "Band",
        .album = "Album"
    };
    int result = check_bandcamp_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, -1);
}

- (void)testCheckBandcampFilesWithFlacFiles {
    const char *exts[] = {"flac"};
    
    // Create a valid Bandcamp folder structure with FLAC files
    char songPath[512];
    for (int i = 1; i <= 3; i++) {
        snprintf(songPath, sizeof(songPath), "%s/Band - Album - %02d Song.flac", self->testDir, i);
        int fd = open(songPath, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {
        .name = "Band",
        .album = "Album"
    };
    int result = check_bandcamp_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, 0);
    XCTAssertEqual(strcmp(info.name, "Band"), 0);
    XCTAssertEqual(strcmp(info.album, "Album"), 0);
    XCTAssertEqual(strcmp(info.file_type, "flac"), 0);
}

- (void)testCheckBandcampFilesWithAacFiles {
    const char *exts[] = {"m4a"};
    
    // Create a valid Bandcamp folder structure with M4A files
    char songPath[512];
    for (int i = 1; i <= 3; i++) {
        snprintf(songPath, sizeof(songPath), "%s/Artist - LP - %02d Track.m4a", self->testDir, i);
        int fd = open(songPath, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {
        .name = "Artist",
        .album = "LP"
    };
    int result = check_bandcamp_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, 0);
    XCTAssertEqual(strcmp(info.name, "Artist"), 0);
    XCTAssertEqual(strcmp(info.album, "LP"), 0);
    XCTAssertEqual(strcmp(info.file_type, "m4a"), 0);
}

- (void)testCheckBandcampFilesMixedTypes {
    const char *exts[] = {"flac", "m4a"};
    
    // Create files with mixed extensions (should fail)
    char songPath[512];
    snprintf(songPath, sizeof(songPath), "%s/Band - Album - 01 Song.flac", self->testDir);
    int fd = open(songPath, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    snprintf(songPath, sizeof(songPath), "%s/Band - Album - 02 Song.m4a", self->testDir);
    fd = open(songPath, O_CREAT|O_WRONLY, 0644);
    close(fd);
    
    band_info_t info = {
        .name = "Band",
        .album = "Album"
    };
    int result = check_bandcamp_files(self->testDir, &info, exts, 2);
    XCTAssertEqual(result, -1);
}

- (void)testCheckBandcampFilesWrongNaming {
    const char *exts[] = {"flac"};
    
    // Create files with wrong naming pattern
    char songPath[512];
    for (int i = 1; i <= 3; i++) {
        snprintf(songPath, sizeof(songPath), "%s/Wrong Name - %02d Song.flac", self->testDir, i);
        int fd = open(songPath, O_CREAT|O_WRONLY, 0644);
        close(fd);
    }
    
    band_info_t info = {
        .name = "Band",
        .album = "Album"
    };
    int result = check_bandcamp_files(self->testDir, &info, exts, 1);
    XCTAssertEqual(result, -1);
}

@end
