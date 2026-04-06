//
//  ConfigTests.m
//  bandcamp_watcherTests
//
//  Tests for config.c functions
//

#import <XCTest/XCTest.h>

#include "config.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

// Expose private functions for testing
extern char *get_xdg_config_path(void);
extern int config_add_mapping(config_t *config, const char *mapping);
extern int is_apple_music_format(const char *ext);

@interface ConfigTests : XCTestCase {
    char testDir[512];
}
@end

@implementation ConfigTests

- (void)setUp {
    [super setUp];
    strcpy(self->testDir, "/tmp/bcw_config_test_XXXXXX");
    XCTAssertTrue(mkdtemp(self->testDir) != NULL, @"Failed to create test directory");
}

- (void)tearDown {
    NSString *cmd = [NSString stringWithFormat:@"rm -rf %s", self->testDir];
    system([cmd UTF8String]);
    [super tearDown];
}

#pragma mark - config_init tests

- (void)testConfigInitSetsDefaults {
    config_t config;
    config_init(&config);
    
    XCTAssertTrue(config.watch_dir == NULL);
    XCTAssertTrue(config.mappings == NULL);
    XCTAssertEqual(config.num_mappings, 0);
    XCTAssertEqual(config.oneshot, 0);
    XCTAssertEqual(config.confirm, 0);
    XCTAssertEqual(config.dry_run, 0);
    XCTAssertEqual(config.apple_music, 1);  // Enabled by default
    XCTAssertEqual(config.log_level, LOG_INFO);
    
    config_free(&config);
}

#pragma mark - config_add_mapping tests

- (void)testAddMappingBasic {
    config_t config;
    config_init(&config);
    
    int result = config_add_mapping(&config, "flac:/path/to/flac");
    XCTAssertEqual(result, 0);
    XCTAssertEqual(config.num_mappings, 1);
    XCTAssertEqual(strcmp(config.mappings[0].ext, "flac"), 0);
    XCTAssertEqual(strcmp(config.mappings[0].target_dir, "/path/to/flac"), 0);
    
    config_free(&config);
}

- (void)testAddMappingStripsLeadingDot {
    config_t config;
    config_init(&config);
    
    int result = config_add_mapping(&config, ".flac:/path/to/flac");
    XCTAssertEqual(result, 0);
    XCTAssertEqual(strcmp(config.mappings[0].ext, "flac"), 0);  // Leading dot removed
    
    config_free(&config);
}

- (void)testAddMappingMultiple {
    config_t config;
    config_init(&config);
    
    XCTAssertEqual(config_add_mapping(&config, "flac:/path/flac"), 0);
    XCTAssertEqual(config_add_mapping(&config, "aac:/path/aac"), 0);
    XCTAssertEqual(config_add_mapping(&config, "mp3:/path/mp3"), 0);
    
    XCTAssertEqual(config.num_mappings, 3);
    XCTAssertEqual(strcmp(config.mappings[0].ext, "flac"), 0);
    XCTAssertEqual(strcmp(config.mappings[1].ext, "aac"), 0);
    XCTAssertEqual(strcmp(config.mappings[2].ext, "mp3"), 0);
    
    config_free(&config);
}

- (void)testAddMappingInvalidNoColon {
    config_t config;
    config_init(&config);
    
    int result = config_add_mapping(&config, "flac");
    XCTAssertEqual(result, -1);
    XCTAssertEqual(config.num_mappings, 0);
    
    config_free(&config);
}

- (void)testAddMappingInvalidEmptyExt {
    config_t config;
    config_init(&config);
    
    int result = config_add_mapping(&config, ":/path/to/dir");
    XCTAssertEqual(result, -1);
    XCTAssertEqual(config.num_mappings, 0);
    
    config_free(&config);
}

- (void)testAddMappingNull {
    config_t config;
    config_init(&config);
    
    int result = config_add_mapping(&config, NULL);
    XCTAssertEqual(result, -1);
    
    config_free(&config);
}

#pragma mark - is_apple_music_format tests

- (void)testAppleMusicFormatAAC {
    XCTAssertTrue(is_apple_music_format("aac"));
    XCTAssertTrue(is_apple_music_format("AAC"));
    XCTAssertTrue(is_apple_music_format("Aac"));
}

- (void)testAppleMusicFormatM4A {
    XCTAssertTrue(is_apple_music_format("m4a"));
    XCTAssertTrue(is_apple_music_format("M4A"));
}

- (void)testAppleMusicFormatMP3 {
    XCTAssertTrue(is_apple_music_format("mp3"));
    XCTAssertTrue(is_apple_music_format("MP3"));
}

- (void)testAppleMusicFormatALAC {
    XCTAssertTrue(is_apple_music_format("alac"));
    XCTAssertTrue(is_apple_music_format("ALAC"));
}

- (void)testAppleMusicFormatFLACNotSupported {
    XCTAssertFalse(is_apple_music_format("flac"));
    XCTAssertFalse(is_apple_music_format("FLAC"));
}

- (void)testAppleMusicFormatWAVNotSupported {
    XCTAssertFalse(is_apple_music_format("wav"));
    XCTAssertFalse(is_apple_music_format("WAV"));
}

- (void)testAppleMusicFormatNull {
    XCTAssertFalse(is_apple_music_format(NULL));
}

- (void)testAppleMusicFormatUnknown {
    XCTAssertFalse(is_apple_music_format("ogg"));
    XCTAssertFalse(is_apple_music_format("opus"));
}

#pragma mark - get_xdg_config_path tests

- (void)testXDGConfigPathWithEnvSet {
    // Save original value
    const char *original = getenv("XDG_CONFIG_HOME");
    char *saved = original ? strdup(original) : NULL;
    
    // Set test value
    setenv("XDG_CONFIG_HOME", "/tmp/test_config", 1);
    
    char *path = get_xdg_config_path();
    XCTAssertTrue(path != NULL);
    XCTAssertTrue(strstr(path, "/tmp/test_config/bandcamp_watcher/config") != NULL);
    
    free(path);
    
    // Restore original
    if (saved) {
        setenv("XDG_CONFIG_HOME", saved, 1);
        free(saved);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }
}

- (void)testXDGConfigPathFallbackToHome {
    // Save original values
    const char *original_xdg = getenv("XDG_CONFIG_HOME");
    const char *original_home = getenv("HOME");
    char *saved_xdg = original_xdg ? strdup(original_xdg) : NULL;
    char *saved_home = original_home ? strdup(original_home) : NULL;
    
    // Unset XDG and set HOME
    unsetenv("XDG_CONFIG_HOME");
    setenv("HOME", "/tmp/test_home", 1);
    
    char *path = get_xdg_config_path();
    XCTAssertTrue(path != NULL);
    XCTAssertTrue(strstr(path, "/tmp/test_home/.config/bandcamp_watcher/config") != NULL);
    
    free(path);
    
    // Restore
    if (saved_xdg) {
        setenv("XDG_CONFIG_HOME", saved_xdg, 1);
        free(saved_xdg);
    }
    if (saved_home) {
        setenv("HOME", saved_home, 1);
        free(saved_home);
    }
}

- (void)testXDGConfigPathReturnsNullIfNoHome {
    // Save original values
    const char *original_xdg = getenv("XDG_CONFIG_HOME");
    const char *original_home = getenv("HOME");
    
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("HOME");
    
    char *path = get_xdg_config_path();
    XCTAssertTrue(path == NULL);
    
    // Restore
    if (original_xdg) setenv("XDG_CONFIG_HOME", original_xdg, 1);
    if (original_home) setenv("HOME", original_home, 1);
}

#pragma mark - config_free tests

- (void)testConfigFreeHandlesNull {
    config_free(NULL);  // Should not crash
}

- (void)testConfigFreeClearsAll {
    config_t config;
    config_init(&config);
    
    config.watch_dir = strdup("/some/path");
    config_add_mapping(&config, "flac:/path/flac");
    config_add_mapping(&config, "aac:/path/aac");
    
    XCTAssertTrue(config.watch_dir != NULL);
    XCTAssertEqual(config.num_mappings, 2);
    
    config_free(&config);
    
    // After free, pointers should be null
    XCTAssertTrue(config.watch_dir == NULL);
    XCTAssertTrue(config.mappings == NULL);
    XCTAssertEqual(config.num_mappings, 0);
}

#pragma mark - config_load file parsing tests

- (void)testLoadConfigFileBasic {
    config_t config;
    config_init(&config);
    
    // Create test config file
    char configPath[512];
    snprintf(configPath, sizeof(configPath), "%s/test_config", self->testDir);
    
    FILE *f = fopen(configPath, "w");
    fprintf(f, "watch_dir = /test/downloads\n");
    fprintf(f, "log_level = debug\n");
    fprintf(f, "apple_music = false\n");
    fprintf(f, "\n");
    fprintf(f, "[extensions]\n");
    fprintf(f, "flac = /test/flac\n");
    fprintf(f, "aac = /test/aac\n");
    fclose(f);
    
    // Load the config file
    int result = config_load_file(&config, configPath);
    XCTAssertEqual(result, 0, @"config_load_file should succeed");
    
    // Verify config values from file
    XCTAssertEqual(strcmp(config.watch_dir, "/test/downloads"), 0, @"watch_dir should match");
    XCTAssertEqual(config.log_level, LOG_DEBUG, @"log_level should be debug");
    XCTAssertEqual(config.apple_music, 0, @"apple_music should be false");
    XCTAssertEqual(config.num_mappings, 2, @"should have 2 mappings");
    XCTAssertEqual(strcmp(config.mappings[0].ext, "flac"), 0, @"first ext should be flac");
    XCTAssertEqual(strcmp(config.mappings[0].target_dir, "/test/flac"), 0, @"first dir should match");
    XCTAssertEqual(strcmp(config.mappings[1].ext, "aac"), 0, @"second ext should be aac");
    XCTAssertEqual(strcmp(config.mappings[1].target_dir, "/test/aac"), 0, @"second dir should match");
    
    // Cleanup
    config_free(&config);
}

#pragma mark - config_validate tests

- (void)testValidateValidConfig {
    config_t config;
    config_init(&config);
    config.watch_dir = strdup("/test/watch");
    config_add_mapping(&config, "flac:/test/flac");
    
    XCTAssertEqual(config_validate(&config), 0, @"Valid config should pass validation");
    
    config_free(&config);
}

- (void)testValidateNullWatchDir {
    config_t config;
    config_init(&config);
    config.watch_dir = NULL;
    config_add_mapping(&config, "flac:/test/flac");
    
    XCTAssertEqual(config_validate(&config), -1, @"NULL watch_dir should fail validation");
    
    config_free(&config);
}

- (void)testValidateEmptyWatchDir {
    config_t config;
    config_init(&config);
    config.watch_dir = strdup("");
    config_add_mapping(&config, "flac:/test/flac");
    
    XCTAssertEqual(config_validate(&config), -1, @"Empty watch_dir should fail validation");
    
    config_free(&config);
}

- (void)testValidateNoMappings {
    config_t config;
    config_init(&config);
    config.watch_dir = strdup("/test/watch");
    // No mappings added
    
    XCTAssertEqual(config_validate(&config), -1, @"No mappings should fail validation");
    
    config_free(&config);
}

- (void)testValidateMappingWithEmptyTarget {
    config_t config;
    config_init(&config);
    config.watch_dir = strdup("/test/watch");
    // Manually add mapping with empty target
    config.mappings = malloc(sizeof(ext_mapping_t));
    config.mappings[0].ext = strdup("flac");
    config.mappings[0].target_dir = strdup("");
    config.num_mappings = 1;
    
    XCTAssertEqual(config_validate(&config), -1, @"Empty target dir should fail validation");
    
    config_free(&config);
}

- (void)testValidateNullConfig {
    XCTAssertEqual(config_validate(NULL), -1, @"NULL config should fail validation");
}

@end
