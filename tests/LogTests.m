//
//  LogTests.m
//  bandcamp_watcherTests
//
//  Tests for log.c functions
//

#import <XCTest/XCTest.h>

#include "log.h"

@interface LogTests : XCTestCase
@end

@implementation LogTests

#pragma mark - parse_log_level tests

- (void)testParseLogLevelDebug {
    XCTAssertEqual(parse_log_level("debug"), LOG_DEBUG);
    XCTAssertEqual(parse_log_level("DEBUG"), LOG_DEBUG);
    XCTAssertEqual(parse_log_level("Debug"), LOG_DEBUG);
}

- (void)testParseLogLevelTrace {
    XCTAssertEqual(parse_log_level("trace"), LOG_TRACE);
    XCTAssertEqual(parse_log_level("TRACE"), LOG_TRACE);
}

- (void)testParseLogLevelInfo {
    XCTAssertEqual(parse_log_level("info"), LOG_INFO);
    XCTAssertEqual(parse_log_level("INFO"), LOG_INFO);
    XCTAssertEqual(parse_log_level("Info"), LOG_INFO);
}

- (void)testParseLogLevelWarn {
    XCTAssertEqual(parse_log_level("warn"), LOG_WARN);
    XCTAssertEqual(parse_log_level("WARN"), LOG_WARN);
    XCTAssertEqual(parse_log_level("warning"), LOG_WARN);
    XCTAssertEqual(parse_log_level("WARNING"), LOG_WARN);
}

- (void)testParseLogLevelError {
    XCTAssertEqual(parse_log_level("error"), LOG_ERROR);
    XCTAssertEqual(parse_log_level("ERROR"), LOG_ERROR);
}

- (void)testParseLogLevelFatal {
    XCTAssertEqual(parse_log_level("fatal"), LOG_FATAL);
    XCTAssertEqual(parse_log_level("FATAL"), LOG_FATAL);
}

- (void)testParseLogLevelNullReturnsInfo {
    XCTAssertEqual(parse_log_level(NULL), LOG_INFO);
}

- (void)testParseLogLevelUnrecognizedReturnsInfo {
    XCTAssertEqual(parse_log_level("verbose"), LOG_INFO);
    XCTAssertEqual(parse_log_level(""), LOG_INFO);
    XCTAssertEqual(parse_log_level("unknown"), LOG_INFO);
}

@end
