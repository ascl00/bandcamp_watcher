//
//  BandcampTests.m
//  bandcamp_watcherTests
//
//  Tests for bandcamp.c functions
//

#import <XCTest/XCTest.h>

#include "bandcamp.h"
#include "utils.h"
#include <sys/stat.h>
#include <unistd.h>

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

#pragma mark - files_with_extension tests

- (void)testFilesWithExtensionEmptyDir {
    size_t count = files_with_extension(self->testDir, ".flac");
    XCTAssertEqual(count, 0);
}

- (void)testFilesWithExtensionCountsMatchingFiles {
    // Create test files
    for (int i = 0; i < 3; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/song%d.flac", self->testDir, i);
        FILE *f = fopen(path, "w");
        fprintf(f, "test");
        fclose(f);
    }
    // Create non-matching file
    char path[512];
    snprintf(path, sizeof(path), "%s/song.txt", self->testDir);
    FILE *f = fopen(path, "w");
    fprintf(f, "test");
    fclose(f);
    
    size_t count = files_with_extension(self->testDir, ".flac");
    XCTAssertEqual(count, 3);
}

- (void)testFilesWithExtensionCaseInsensitive {
    char path[512];
    snprintf(path, sizeof(path), "%s/song.FLAC", self->testDir);
    FILE *f = fopen(path, "w");
    fprintf(f, "test");
    fclose(f);
    
    size_t count = files_with_extension(self->testDir, ".flac");
    XCTAssertEqual(count, 1);
}

#pragma mark - band_info_string tests

- (void)testBandInfoStringFlac {
    band_info_t info = {
        .name = "Test Band",
        .album = "Test Album",
        .file_type = flac
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
        .file_type = aac
    };
    const char *result = band_info_string(&info);
    XCTAssertTrue(strstr(result, "aac") != NULL);
}

#pragma mark - check_for_bandcamp_folder tests

- (void)testCheckBandcampFolderNoDash {
    band_info_t info;
    int result = check_for_bandcamp_folder(self->testDir, "NoDashFolder", &info);
    XCTAssertEqual(result, -1);
}

- (void)testCheckBandcampFolderNoAudioFiles {
    band_info_t info;
    int result = check_for_bandcamp_folder(self->testDir, "Band - Album", &info);
    XCTAssertEqual(result, -1);
}

- (void)testCheckBandcampFolderWithFlacFiles {
    // Create a valid Bandcamp folder structure with FLAC files
    char songPath[512];
    snprintf(songPath, sizeof(songPath), "%s/Band - Album - 01 Song.flac", self->testDir);
    FILE *f = fopen(songPath, "w");
    fprintf(f, "test");
    fclose(f);
    
    band_info_t info;
    int result = check_for_bandcamp_folder(self->testDir, "Band - Album", &info);
    XCTAssertEqual(result, 0);
    XCTAssertEqual(strcmp(info.name, "Band"), 0);
    XCTAssertEqual(strcmp(info.album, "Album"), 0);
    XCTAssertEqual(info.file_type, flac);
}

- (void)testCheckBandcampFolderWithAacFiles {
    // Create a valid Bandcamp folder structure with M4A files
    char songPath[512];
    snprintf(songPath, sizeof(songPath), "%s/Artist - LP - 01 Track.m4a", self->testDir);
    FILE *f = fopen(songPath, "w");
    fprintf(f, "test");
    fclose(f);
    
    band_info_t info;
    int result = check_for_bandcamp_folder(self->testDir, "Artist - LP", &info);
    XCTAssertEqual(result, 0);
    XCTAssertEqual(strcmp(info.name, "Artist"), 0);
    XCTAssertEqual(strcmp(info.album, "LP"), 0);
    XCTAssertEqual(info.file_type, aac);
}

- (void)testCheckBandcampFolderStripsTrailingNumber {
    // Test folder name like "Band - Album-2" (Safari duplicate)
    char songPath[512];
    snprintf(songPath, sizeof(songPath), "%s/Band - Album - 01 Song.flac", self->testDir);
    FILE *f = fopen(songPath, "w");
    fprintf(f, "test");
    fclose(f);
    
    band_info_t info;
    int result = check_for_bandcamp_folder(self->testDir, "Band - Album-2", &info);
    XCTAssertEqual(result, 0);
    XCTAssertEqual(strcmp(info.album, "Album"), 0);
}

@end
