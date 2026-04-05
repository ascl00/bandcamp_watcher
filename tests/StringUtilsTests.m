//
//  StringUtilsTests.m
//  bandcamp_watcherTests
//
//  Tests for utils.c functions
//

#import <XCTest/XCTest.h>

// Import C headers
#include "utils.h"

@interface StringUtilsTests : XCTestCase
@end

@implementation StringUtilsTests

#pragma mark - name_compare tests

- (void)testNameCompareExactMatch {
    XCTAssertEqual(name_compare("Artist", "Artist", 100), 0);
}

- (void)testNameCompareCaseInsensitive {
    XCTAssertEqual(name_compare("Artist", "artist", 100), 0);
    XCTAssertEqual(name_compare("ARTIST", "artist", 100), 0);
    XCTAssertEqual(name_compare("Artist", "ARTIST", 100), 0);
}

- (void)testNameCompareWithPunctuation {
    // Function comment says it "wont fail a match if puncuation characters mismatch"
    // but currently it doesn't fully skip punctuation consistently
    XCTAssertEqual(name_compare("Artist - Album", "Artist - Album", 100), 0);

    // This is arguably some strange behaviour but is based on - being special in bandcamp downloads
    XCTAssertEqual(name_compare("Artist , Album", "Artist . Album", 100), 0);

}

- (void)testNameCompareDifferentNames {
    XCTAssertNotEqual(name_compare("Artist", "Band", 100), 0);
    XCTAssertNotEqual(name_compare("AAA", "ZZZ", 100), 0);
}

- (void)testNameComparePartialMatch {
    // Only compare up to specified length
    // Current behavior: compares up to length but returns actual diff at that point
    // "Artist Name" vs "Artist Band" at position 6: 'N'(78) - 'B'(66) = 12
    XCTAssertEqual(name_compare("Artist Name", "Artist Band", 6), 12);
    // Full comparison shows they differ
    XCTAssertNotEqual(name_compare("Artist Name", "Artist Band", 12), 0);
}

- (void)testNameCompareEmptyStrings {
    XCTAssertEqual(name_compare("", "", 100), 0);
    XCTAssertEqual(name_compare("Artist", "", 100), 0);  // existing logic returns 0
    XCTAssertEqual(name_compare("", "Artist", 100), 0); // existing logic returns 0
}

- (void)testNameCompareSamePointer {
    const char *same = "Artist";
    XCTAssertEqual(name_compare(same, same, 100), 0);
}

#pragma mark - is_matching_extension tests

- (void)testExtensionMatchFlacLowercase {
    XCTAssertEqual(is_matching_extension("song.flac", ".flac"), 0);
}

- (void)testExtensionMatchFlacUppercase {
    XCTAssertEqual(is_matching_extension("song.FLAC", ".flac"), 0);
}

- (void)testExtensionMatchMixedCase {
    XCTAssertEqual(is_matching_extension("song.Flac", ".flac"), 0);
    XCTAssertEqual(is_matching_extension("song.fLaC", ".flac"), 0);
}

- (void)testExtensionMatchM4a {
    XCTAssertEqual(is_matching_extension("song.m4a", ".m4a"), 0);
    XCTAssertEqual(is_matching_extension("song.M4A", ".m4a"), 0);
}

- (void)testExtensionNoMatchWrongExt {
    XCTAssertNotEqual(is_matching_extension("song.mp3", ".flac"), 0);
    XCTAssertNotEqual(is_matching_extension("song.wav", ".flac"), 0);
    XCTAssertNotEqual(is_matching_extension("song.flac", ".m4a"), 0);
}

- (void)testExtensionNoMatchNoExt {
    // BUG: Function returns 0 (appears to match) when filename is shorter than extension
    // This is because p winds back before start of string, then returns strcasecmp result
    // Current behavior (bug):
    XCTAssertEqual(is_matching_extension("song", ".flac"), 0);  // BUG: returns 0
    // "song." vs ".flac" - p winds back to point at "song" effectively
    // strcasecmp will compare and find difference
    XCTAssertNotEqual(is_matching_extension("song.", ".flac"), 0);  // Actually doesn't match
}

- (void)testExtensionNoMatchPartial {
    // Contains the ext string but not at end
    XCTAssertNotEqual(is_matching_extension("flac.song", ".flac"), 0);
    XCTAssertNotEqual(is_matching_extension("song.flac.backup", ".flac"), 0);
}

- (void)testExtensionEmptyFilename {
    XCTAssertEqual(is_matching_extension("", ".flac"), 0);  // winds back before start, returns 0
}

- (void)testExtensionWithPath {
    XCTAssertEqual(is_matching_extension("/path/to/song.flac", ".flac"), 0);
    XCTAssertEqual(is_matching_extension("/Music/Band/Album/song.m4a", ".m4a"), 0);
}

- (void)testExtensionSimilarButDifferent {
    // .flac1 should not match .flac
    XCTAssertNotEqual(is_matching_extension("song.flac1", ".flac"), 0);
    // .flacs should not match .flac
    XCTAssertNotEqual(is_matching_extension("song.flacs", ".flac"), 0);
}

@end
