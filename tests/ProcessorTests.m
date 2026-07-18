#import <XCTest/XCTest.h>

#define BCW_PROCESSOR_TESTS 1
#include "../bandcamp_watcher/main.c"

@interface ProcessorTests : XCTestCase {
    char root[PATH_MAX];
    char watch[PATH_MAX];
    char target[PATH_MAX];
}
@end

@implementation ProcessorTests

- (void)setUp {
    [super setUp];
    strcpy(root, "/tmp/bcw_processor_test_XXXXXX");
    XCTAssertNotEqual(mkdtemp(root), NULL);
    snprintf(watch, sizeof(watch), "%s/watch", root);
    snprintf(target, sizeof(target), "%s/target", root);
    XCTAssertEqual(mkdir(watch, 0755), 0);
    XCTAssertEqual(mkdir(target, 0755), 0);
}

- (void)tearDown {
    NSString *command = [NSString stringWithFormat:@"rm -rf %s", root];
    system(command.UTF8String);
    [super tearDown];
}

- (void)createAlbum {
    char album[PATH_MAX];
    snprintf(album, sizeof(album), "%s/Artist - Album", watch);
    XCTAssertEqual(mkdir(album, 0755), 0);
    for (int i = 1; i <= 3; i++) {
        char track[PATH_MAX];
        snprintf(track, sizeof(track), "%s/Artist - Album - %02d Track.flac", album, i);
        FILE *file = fopen(track, "w");
        XCTAssertNotEqual(file, NULL);
        if (file) { fputs("audio", file); fclose(file); }
    }
}

- (config_t)configWithDryRun:(BOOL)dryRun {
    config_t config;
    config_init(&config);
    config.watch_dir = strdup(watch);
    char mapping[PATH_MAX + 16];
    snprintf(mapping, sizeof(mapping), "flac:%s", target);
    XCTAssertEqual(config_add_mapping(&config, mapping), 0);
    config.dry_run = dryRun;
    config.apple_music = 0;
    return config;
}

- (void)testProcessCopiesRecognizedAlbum {
    [self createAlbum];
    config_t config = [self configWithDryRun:NO];
    context_t context = {.config = &config, .last_run = {0, 0}, .state_db = NULL};
    process_result_t result = process(&context);
    XCTAssertEqual(result.status, PROCESS_COMPLETED);
    XCTAssertEqual(result.error_count, 0u);
    char copied[PATH_MAX];
    snprintf(copied, sizeof(copied), "%s/Artist/Album/Artist - Album - 01 Track.flac", target);
    XCTAssertEqual(access(copied, F_OK), 0);
    config_free(&config);
}

- (void)testDryRunMakesNoDestinationChanges {
    [self createAlbum];
    config_t config = [self configWithDryRun:YES];
    context_t context = {.config = &config, .last_run = {0, 0}, .state_db = NULL};
    process_result_t result = process(&context);
    XCTAssertEqual(result.error_count, 0u);
    char artist[PATH_MAX];
    snprintf(artist, sizeof(artist), "%s/Artist", target);
    XCTAssertFalse(dir_exists(artist));
    config_free(&config);
}

- (void)testExistingDestinationIsSkipped {
    [self createAlbum];
    char artist[PATH_MAX], album[PATH_MAX];
    snprintf(artist, sizeof(artist), "%s/Artist", target);
    snprintf(album, sizeof(album), "%s/Album", artist);
    XCTAssertEqual(mkdir(artist, 0755), 0);
    XCTAssertEqual(mkdir(album, 0755), 0);
    config_t config = [self configWithDryRun:NO];
    context_t context = {.config = &config, .last_run = {0, 0}, .state_db = NULL};
    process_result_t result = process(&context);
    XCTAssertEqual(result.error_count, 0u);
    config_free(&config);
}

- (void)testProcessReportsMissingWatchDirectory {
    config_t config = [self configWithDryRun:NO];
    free(config.watch_dir);
    config.watch_dir = strdup("/tmp/no-such-bcw-watch");
    context_t context = {.config = &config, .last_run = {0, 0}, .state_db = NULL};
    XCTAssertEqual(process(&context).status, PROCESS_FATAL);
    config_free(&config);
}

@end
