//
//  FolderTests.m
//  bandcamp_watcherTests
//
//  Tests for folder.c functions
//

#import <XCTest/XCTest.h>

#include "folder.h"

@interface FolderTests : XCTestCase
@end

@implementation FolderTests

#pragma mark - parse_folder_name tests

- (void)testParseBasicFolderName {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("Artist - Album", name, sizeof(name), album, sizeof(album)), 0);
    XCTAssertEqual(strcmp(name, "Artist"), 0);
    XCTAssertEqual(strcmp(album, "Album"), 0);
}

- (void)testParseFolderNameWithSpaces {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("Pink Floyd - Dark Side of the Moon", name, sizeof(name), album, sizeof(album)), 0);
    XCTAssertEqual(strcmp(name, "Pink Floyd"), 0);
    XCTAssertEqual(strcmp(album, "Dark Side of the Moon"), 0);
}

- (void)testParseFolderNameWithDuplicateSuffix {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("Artist - Album-2", name, sizeof(name), album, sizeof(album)), 0);
    XCTAssertEqual(strcmp(name, "Artist"), 0);
    XCTAssertEqual(strcmp(album, "Album"), 0);
}

- (void)testParseFolderNameWithHighDuplicateNumber {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("Artist - Album-10", name, sizeof(name), album, sizeof(album)), 0);
    XCTAssertEqual(strcmp(name, "Artist"), 0);
    XCTAssertEqual(strcmp(album, "Album"), 0);
}

- (void)testParseFolderNameWithPreOrder {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("Artist - Album (pre-order)", name, sizeof(name), album, sizeof(album)), 0);
    XCTAssertEqual(strcmp(name, "Artist"), 0);
    XCTAssertEqual(strcmp(album, "Album"), 0);
}

- (void)testParseFolderNameWithDuplicateAndPreOrder {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("Artist - Album (pre-order)-3", name, sizeof(name), album, sizeof(album)), 0);
    XCTAssertEqual(strcmp(name, "Artist"), 0);
    XCTAssertEqual(strcmp(album, "Album"), 0);
}

- (void)testParseFolderNameNoDash {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("JustAName", name, sizeof(name), album, sizeof(album)), -1);
}

- (void)testParseFolderNameDashAtStart {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("- Album", name, sizeof(name), album, sizeof(album)), -1);
}

- (void)testParseFolderNameDashAtEnd {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("Artist -", name, sizeof(name), album, sizeof(album)), -1);
}

- (void)testParseFolderNameNoSpaceAfterDash {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("Artist-Album", name, sizeof(name), album, sizeof(album)), -1);
}

- (void)testParseFolderNameOnlyNumbersAfterDash {
    char name[256];
    char album[256];
    
    // This is "Artist -123" which looks like a duplicate suffix
    XCTAssertEqual(parse_folder_name("Artist -123", name, sizeof(name), album, sizeof(album)), -1);
}

- (void)testParseFolderNameTrailingWhitespace {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("Artist - Album   ", name, sizeof(name), album, sizeof(album)), 0);
    XCTAssertEqual(strcmp(name, "Artist"), 0);
    XCTAssertEqual(strcmp(album, "Album"), 0);
}

- (void)testParseFolderNameWithSpecialChars {
    char name[256];
    char album[256];
    
    XCTAssertEqual(parse_folder_name("AC/DC - Highway to Hell", name, sizeof(name), album, sizeof(album)), 0);
    XCTAssertEqual(strcmp(name, "AC/DC"), 0);
    XCTAssertEqual(strcmp(album, "Highway to Hell"), 0);
}

- (void)testParseFolderNameLongNames {
    char name[512];
    char album[512];
    
    XCTAssertEqual(parse_folder_name("A Very Long Artist Name That Has Many Words - An Even Longer Album Title That Could Be Very Descriptive", name, sizeof(name), album, sizeof(album)), 0);
    XCTAssertTrue(strstr(name, "Very Long Artist Name") != NULL);
    XCTAssertTrue(strstr(album, "Even Longer Album Title") != NULL);
}

@end
