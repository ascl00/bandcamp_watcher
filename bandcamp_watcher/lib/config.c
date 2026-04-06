//
//  config.c
//  bandcamp_watcher
//
//  Configuration management implementation
//

#include "config.h"
#include "utils.h"
#include "copy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

// Apple Music supported format whitelist
static const char *apple_music_formats[] = {
    "aac", "m4a", "mp3", "alac", NULL
};

// Default extension mappings (used if none specified)
static const struct {
    const char *ext;
    const char *dir;
} default_mappings[] = {
    {"flac", "/Volumes/Multimedia/Music/FLAC"},
    {"aac", "/Volumes/Multimedia/Music/aac"},
    {"mp3", "/Volumes/Multimedia/Music/aac"},
    {NULL, NULL}
};

// Check if extension is in Apple Music whitelist
int is_apple_music_format(const char *ext) {
    if (!ext) return 0;
    for (int i = 0; apple_music_formats[i]; i++) {
        if (strcasecmp(ext, apple_music_formats[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Initialize config with default values
void config_init(config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(config_t));
    
    // Set defaults
    config->watch_dir = NULL;  // Will be set to ~/Downloads
    config->mappings = NULL;
    config->num_mappings = 0;
    config->oneshot = 0;
    config->confirm = 0;
    config->dry_run = 0;
    config->apple_music = 1;  // Enabled by default
    config->log_level = LOG_INFO;
}

// Get XDG config path
char *get_xdg_config_path(void) {
    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    char *path = NULL;
    
    if (xdg_config && *xdg_config) {
        path = malloc(strlen(xdg_config) + strlen("/bandcamp_watcher/config") + 1);
        if (path) {
            sprintf(path, "%s/bandcamp_watcher/config", xdg_config);
        }
    } else {
        const char *home = getenv("HOME");
        if (home && *home) {
            path = malloc(strlen(home) + strlen("/.config/bandcamp_watcher/config") + 1);
            if (path) {
                sprintf(path, "%s/.config/bandcamp_watcher/config", home);
            }
        }
    }
    
    return path;
}

// Parse extension:mapping pair (format: "flac:/path/to/dir")
int config_add_mapping(config_t *config, const char *mapping) {
    if (!config || !mapping) return -1;
    
    char *colon = strchr(mapping, ':');
    if (!colon) {
        fprintf(stderr, "Error: Invalid mapping format '%s' (expected ext:dir)\n", mapping);
        return -1;
    }
    
    size_t ext_len = colon - mapping;
    if (ext_len == 0) {
        fprintf(stderr, "Error: Empty extension in mapping '%s'\n", mapping);
        return -1;
    }
    
    // Expand array
    ext_mapping_t *new_mappings = realloc(config->mappings, 
                                          (config->num_mappings + 1) * sizeof(ext_mapping_t));
    if (!new_mappings) {
        fprintf(stderr, "Error: Out of memory\n");
        return -1;
    }
    
    config->mappings = new_mappings;
    ext_mapping_t *m = &config->mappings[config->num_mappings];
    
    // Zero out the new entry so config_free can handle partial failures
    m->ext = NULL;
    m->target_dir = NULL;
    
    // Copy extension (without leading dot if present)
    m->ext = malloc(ext_len + 1);
    if (!m->ext) {
        fprintf(stderr, "Error: Out of memory\n");
        return -1;
    }
    strncpy(m->ext, mapping, ext_len);
    m->ext[ext_len] = '\0';
    
    // Remove leading dot if present
    if (m->ext[0] == '.') {
        memmove(m->ext, m->ext + 1, ext_len);  // includes null terminator
    }
    
    // Copy directory path
    m->target_dir = strdup(colon + 1);
    if (!m->target_dir) {
        fprintf(stderr, "Error: Out of memory\n");
        free(m->ext);
        m->ext = NULL;
        return -1;
    }
    
    config->num_mappings++;
    return 0;
}

// Load config from file
// Returns 0 on success (file may not exist), -1 on parse error
static int load_config_file(config_t *config, const char *path) {
    if (!path) return 0;  // No file to load
    
    FILE *f = fopen(path, "r");
    if (!f) {
        if (errno != ENOENT) {
            perror(path);
        }
        return 0;  // File doesn't exist is OK
    }
    
    char line[1024];
    int in_extensions_section = 0;
    int line_num = 0;
    
    while (fgets(line, sizeof(line), f)) {
        line_num++;
        
        // Remove comment
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        
        // Trim
        char *trimmed = trim(line);
        if (*trimmed == '\0') continue;  // Empty line
        
        // Check for section
        if (trimmed[0] == '[') {
            char *end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                char *section = trimmed + 1;
                in_extensions_section = (strcasecmp(section, "extensions") == 0);
            }
            continue;
        }
        
        // Parse key=value
        char *equals = strchr(trimmed, '=');
        if (!equals) {
            fprintf(stderr, "Warning: %s:%d: Expected key=value\n", path, line_num);
            continue;
        }
        
        *equals = '\0';
        char *key = trim(trimmed);
        char *value = trim(equals + 1);
        
        if (in_extensions_section) {
            // Extension mapping: ext = /path/to/dir
            // Buffer needs to fit: ext + ":" + path (up to NAME_MAX) + null
            char mapping[NAME_MAX+256];
            snprintf(mapping, sizeof(mapping), "%s:%s", key, value);
            if (config_add_mapping(config, mapping) != 0) {
                fclose(f);
                return -1;
            }
        } else {
            // Global setting
            if (strcasecmp(key, "watch_dir") == 0) {
                free(config->watch_dir);
                config->watch_dir = strdup(value);
            } else if (strcasecmp(key, "log_level") == 0) {
                config->log_level = parse_log_level(value);
            } else if (strcasecmp(key, "apple_music") == 0) {
                config->apple_music = parse_bool(value);
            }
            // Ignore unknown keys
        }
    }
    
    fclose(f);
    return 0;
}

// Set watch directory with validation
// Returns 0 on success, -1 on error (directory doesn't exist or allocation failed)
int config_set_watch_dir(config_t *config, const char *path) {
    if (!path || !*path) {
        fprintf(stderr, "Error: Empty watch directory path\n");
        return -1;
    }
    
    // Expand ~ to HOME if present
    char *expanded_path = NULL;
    if (path[0] == '~' && path[1] == '/') {
        const char *home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "Error: Cannot expand ~ - HOME not set\n");
            return -1;
        }
        expanded_path = malloc(strlen(home) + strlen(path) + 1);
        if (!expanded_path) {
            fprintf(stderr, "Error: Out of memory\n");
            return -1;
        }
        sprintf(expanded_path, "%s%s", home, path + 1);
        path = expanded_path;
    }
    
    // Verify the directory exists
    if (!dir_exists(path)) {
        fprintf(stderr, "Error: Watch directory does not exist: %s\n", path);
        fprintf(stderr, "  Please create it or specify a different directory\n");
        free(expanded_path);
        return -1;
    }
    
    // Free any existing watch_dir
    free(config->watch_dir);
    
    // Duplicate the path
    if (expanded_path) {
        config->watch_dir = expanded_path;  // Already allocated
    } else {
        config->watch_dir = strdup(path);
        if (!config->watch_dir) {
            fprintf(stderr, "Error: Out of memory\n");
            return -1;
        }
    }
    
    return 0;
}

// Set default watch directory
// Returns 0 on success, -1 on error (directory doesn't exist or allocation failed)
static int set_default_watch_dir(config_t *config) {
    if (config->watch_dir) return 0;  // Already set
    
    const char *home = getenv("HOME");
    if (!home || !*home) {
        fprintf(stderr, "Error: HOME environment variable not set\n");
        return -1;
    }
    
    char default_path[NAME_MAX+1];
    snprintf(default_path, sizeof(default_path), "%s/Downloads", home);
    
    return config_set_watch_dir(config, default_path);
}

// Set default mappings if none provided
// Returns 0 on success, -1 on error
static int set_default_mappings(config_t *config) {
    if (config->num_mappings > 0) return 0;  // Already have mappings
    
    for (int i = 0; default_mappings[i].ext; i++) {
        // Buffer needs to fit: ext + ":" + path (up to NAME_MAX) + null
        char mapping[NAME_MAX+256];
        snprintf(mapping, sizeof(mapping), "%s:%s", 
                 default_mappings[i].ext, default_mappings[i].dir);
        if (config_add_mapping(config, mapping) != 0) {
            return -1;  // Failed to add mapping
        }
    }
    return 0;
}

// Load configuration from file and apply defaults
int config_load_file(config_t *config, const char *config_file_path) {
    if (!config) return -1;
    
    // Initialize with defaults
    config_init(config);
    
    // Get config file path
    char *xdg_path = NULL;
    const char *path = config_file_path;
    
    if (!path) {
        xdg_path = get_xdg_config_path();
        path = xdg_path;
    }
    
    // Load config file (if it exists)
    if (path && load_config_file(config, path) != 0) {
        free(xdg_path);
        return -1;
    }
    
    free(xdg_path);
    
    // Apply defaults for anything not set by config file
    if (set_default_watch_dir(config) != 0) {
        return -1;  // Failed to set default watch directory
    }
    if (set_default_mappings(config) != 0) {
        return -1;  // Failed to set default mappings
    }
    
    return 0;
}

// Validate configuration after CLI args have been applied
int config_validate(const config_t *config) {
    if (!config) {
        fprintf(stderr, "Error: No configuration provided\n");
        return -1;
    }
    
    if (!config->watch_dir || !*config->watch_dir) {
        fprintf(stderr, "Error: No watch directory specified\n");
        fprintf(stderr, "  Set watch_dir in config file or use -w/--watch option\n");
        return -1;
    }
    
    if (config->num_mappings == 0) {
        fprintf(stderr, "Error: No extension mappings specified\n");
        fprintf(stderr, "  Add mappings in config file [extensions] section or use -e/--ext option\n");
        return -1;
    }
    
    // Check that all mappings have valid target directories
    for (int i = 0; i < config->num_mappings; i++) {
        if (!config->mappings[i].target_dir || !*config->mappings[i].target_dir) {
            fprintf(stderr, "Error: Extension '%s' has no target directory\n", 
                    config->mappings[i].ext);
            return -1;
        }
    }
    
    return 0;
}

// Free all allocated memory
void config_free(config_t *config) {
    if (!config) return;
    
    free(config->watch_dir);
    config->watch_dir = NULL;
    
    for (int i = 0; i < config->num_mappings; i++) {
        free(config->mappings[i].ext);
        free(config->mappings[i].target_dir);
    }
    free(config->mappings);
    config->mappings = NULL;
    config->num_mappings = 0;
}

// Print configuration for debugging
void config_print(const config_t *config) {
    if (!config) return;
    
    printf("Configuration:\n");
    printf("  Watch directory: %s\n", config->watch_dir ? config->watch_dir : "(not set)");
    printf("  Log level: %d\n", config->log_level);
    printf("  Oneshot: %s\n", config->oneshot ? "yes" : "no");
    printf("  Confirm: %s\n", config->confirm ? "yes" : "no");
    printf("  Dry run: %s\n", config->dry_run ? "yes" : "no");
    printf("  Apple Music: %s\n", config->apple_music ? "yes" : "no");
    printf("  Extension mappings (%d):\n", config->num_mappings);
    
    for (int i = 0; i < config->num_mappings; i++) {
        printf("    %s -> %s\n", config->mappings[i].ext, config->mappings[i].target_dir);
    }
}
