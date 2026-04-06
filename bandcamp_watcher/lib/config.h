//
//  config.h
//  bandcamp_watcher
//
//  Configuration management - XDG config files and CLI arguments
//

#ifndef CONFIG_H
#define CONFIG_H

#include "log.h"
#include <stddef.h>

// Extension to directory mapping
typedef struct {
    char *ext;           // e.g., "flac", "aac"
    char *target_dir;    // destination directory path
} ext_mapping_t;

// Main configuration structure
typedef struct {
    char *watch_dir;           // Directory to watch (default: ~/Downloads)
    ext_mapping_t *mappings;   // Dynamic array of extension mappings
    int num_mappings;          // Count of mappings
    int oneshot;              // Bool: run once and exit
    int confirm;              // Bool: prompt before each action
    int dry_run;              // Bool: simulate only, no changes
    int apple_music;          // Bool: add to Apple Music (default: true)
    log_level_t log_level;    // Logging verbosity
} config_t;

// Initialize config with default values
void config_init(config_t *config);

// Load configuration from file and apply defaults
// If config_file_path is NULL, uses XDG default location
// Returns 0 on success, -1 on error
int config_load_file(config_t *config, const char *config_file_path);

// Validate configuration after CLI args have been applied
// Returns 0 if valid, -1 if invalid (logs error details)
int config_validate(const config_t *config);

// Free all allocated memory in config
void config_free(config_t *config);

// Print configuration for debugging
void config_print(const config_t *config);

// Get XDG config file path (returns malloc'd string, caller must free)
char *get_xdg_config_path(void);

// Parse a single extension:mapping pair
// Returns 0 on success, -1 on error
int config_add_mapping(config_t *config, const char *mapping);

// Set watch directory with validation (checks that directory exists)
// Supports ~ expansion for HOME directory
// Returns 0 on success, -1 on error
int config_set_watch_dir(config_t *config, const char *path);

// Apple Music supported formats whitelist
int is_apple_music_format(const char *ext);

#endif /* CONFIG_H */
