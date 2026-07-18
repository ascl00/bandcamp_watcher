#import <XCTest/XCTest.h>

#include "args.h"
#include <unistd.h>

@interface ArgsTests : XCTestCase {
    char testDir[PATH_MAX];
}
@end

@implementation ArgsTests

- (void)setUp {
    [super setUp];
    strcpy(testDir, "/tmp/bcw_args_test_XXXXXX");
    XCTAssertNotEqual(mkdtemp(testDir), NULL);
}

- (void)tearDown {
    rmdir(testDir);
    [super tearDown];
}

- (void)testParsesAllBooleanAndLogOptions {
    config_t config;
    config_init(&config);
    char *argv[] = {"bcw", "--oneshot", "--confirm", "--no-apple-music", "--verbose"};
    XCTAssertEqual(parse_args(&config, 5, argv), 0);
    XCTAssertTrue(config.oneshot);
    XCTAssertTrue(config.confirm);
    XCTAssertFalse(config.apple_music);
    XCTAssertEqual(config.log_level, LOG_DEBUG);
    config_free(&config);
}

- (void)testDryRunImpliesOneshot {
    config_t config;
    config_init(&config);
    char *argv[] = {"bcw", "--dry-run"};
    XCTAssertEqual(parse_args(&config, 2, argv), 0);
    XCTAssertTrue(config.dry_run);
    XCTAssertTrue(config.oneshot);
    config_free(&config);
}

- (void)testWatchAndRepeatedMappingsOverrideExistingValues {
    config_t config;
    config_init(&config);
    config.watch_dir = strdup("/old");
    config_add_mapping(&config, "wav:/old/wav");
    char watchArg[PATH_MAX];
    strcpy(watchArg, testDir);
    char *argv[] = {"bcw", "--watch", watchArg, "--ext", "flac:/music/flac", "-e", "aac:/music/aac"};
    XCTAssertEqual(parse_args(&config, 7, argv), 0);
    XCTAssertEqual(strcmp(config.watch_dir, testDir), 0);
    XCTAssertEqual(config.num_mappings, 3);
    config_free(&config);
}

- (void)testQuietWinsWhenSpecifiedAfterVerbose {
    config_t config;
    config_init(&config);
    char *argv[] = {"bcw", "--verbose", "--quiet"};
    XCTAssertEqual(parse_args(&config, 3, argv), 0);
    XCTAssertEqual(config.log_level, LOG_ERROR);
    config_free(&config);
}

- (void)testConsumesConfigOption {
    config_t config;
    config_init(&config);
    char *argv[] = {"bcw", "--config", "/tmp/config"};
    XCTAssertEqual(parse_args(&config, 3, argv), 0);
    config_free(&config);
}

- (void)testRejectsMissingValuesAndPositionals {
    config_t config;
    config_init(&config);
    char *missing[] = {"bcw", "--watch"};
    XCTAssertEqual(parse_args(&config, 2, missing), -1);
    char *positional[] = {"bcw", "unexpected"};
    XCTAssertEqual(parse_args(&config, 2, positional), -1);
    config_free(&config);
}

@end
