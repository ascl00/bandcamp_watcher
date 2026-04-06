//
//  main.c
//  bandcamp_watcher
//
//  Created by Nick Blievers on 18/6/2025.
//

#include <errno.h>       // for errno
#include <fcntl.h>       // for O_RDONLY
#include <stdio.h>       // for fprintf()
#include <stdlib.h>      // for EXIT_SUCCESS
#include <ctype.h>
#include <string.h>      // for strerror()
#include <sys/event.h>   // for kqueue() etc.
#include <unistd.h>      // for close()
#include <dirent.h>      // for opendir()
#include <sys/stat.h>    // for stat()
#include <sys/time.h>    // for timespec_diff_macro()
#include <limits.h>

#include "log.h"         // logging functions
#include "copy.h"
#include "utils.h"
#include "bandcamp.h"
#include "config.h"
#include "args.h"

typedef struct {
    config_t *config;
    struct timeval last_run;  // last time we processed an event
} context_t;

char *flagstring(int flags)
{
    static char ret[512];
    char *or = "";
 
    ret[0]='\0'; // clear the string.
    if (flags & NOTE_DELETE) {strcat(ret,or);strcat(ret,"NOTE_DELETE");or="|";}
    if (flags & NOTE_WRITE) {strcat(ret,or);strcat(ret,"NOTE_WRITE");or="|";}
    if (flags & NOTE_EXTEND) {strcat(ret,or);strcat(ret,"NOTE_EXTEND");or="|";}
    if (flags & NOTE_ATTRIB) {strcat(ret,or);strcat(ret,"NOTE_ATTRIB");or="|";}
    if (flags & NOTE_LINK) {strcat(ret,or);strcat(ret,"NOTE_LINK");or="|";}
    if (flags & NOTE_RENAME) {strcat(ret,or);strcat(ret,"NOTE_RENAME");or="|";}
    if (flags & NOTE_REVOKE) {strcat(ret,or);strcat(ret,"NOTE_REVOKE");or="|";}
 
    return ret;
}

int add_folder_to_apple_music(const char *folder)
{
    char cmd[NAME_MAX+1];
    sprintf(cmd, "/usr/bin/osascript -e \"tell application \\\"Music\\\" to add Posix file \\\"%s\\\"\"", folder);
    return system(cmd);
}

// Get target directory for a file type extension
static const char *get_target_dir(const config_t *config, const char *ext)
{
    if (!config || !ext) return NULL;
    
    for (int i = 0; i < config->num_mappings; i++) {
        if (strcasecmp(ext, config->mappings[i].ext) == 0) {
            return config->mappings[i].target_dir;
        }
    }
    return NULL;
}

// Show confirmation prompt for a folder
// Returns: 1 = process, 0 = skip, -1 = quit
static int confirm_action(const char *folder_name, const band_info_t *info, 
                          const char *src_path, const char *dst_path,
                          int dry_run)
{
    printf("%sFound: %s (%s)\n", dry_run ? "[DRY RUN] " : "", folder_name,
           info->file_type == flac ? "flac" : 
           info->file_type == aac ? "aac" : "unknown");
    printf("  Source: %s\n", src_path);
    printf("  Destination: %s\n", dst_path);
    
    if (dry_run) {
        printf("  Would copy files\n");
        if (is_apple_music_format(info->file_type == flac ? "flac" : "aac")) {
            printf("  Would add to Apple Music\n");
        } else {
            printf("  Would NOT add to Apple Music (format not supported)\n");
        }
        return 1;  // In dry-run, always "process" (but don't actually do it)
    }
    
    printf("\nProcess this album? [y/n/s/q] ");
    fflush(stdout);
    
    char response[10];
    if (fgets(response, sizeof(response), stdin) == NULL) {
        return -1;  // EOF or error
    }
    
    // Remove newline
    response[strcspn(response, "\n")] = '\0';
    
    if (response[0] == 'y' || response[0] == 'Y') {
        return 1;
    } else if (response[0] == 'n' || response[0] == 'N') {
        return 0;
    } else if (response[0] == 's' || response[0] == 'S') {
        return 0;  // Skip remaining
    } else if (response[0] == 'q' || response[0] == 'Q') {
        return -1;  // Quit
    }
    
    // Default to yes if unclear
    return 1;
}

int process(context_t *context)
{
    config_t *config = context->config;
    if (!config) return -1;
    
    struct timeval start_of_run;
    gettimeofday(&start_of_run, NULL);
    
    DIR *dirp = opendir(config->watch_dir);
    struct dirent *dp = NULL;
    
    if (dirp == NULL) {
        log_error("Failed to open watch directory %s: %s", config->watch_dir, strerror(errno));
        return -1;
    }
    
    int skip_remaining = 0;
    int quit = 0;
    
    while ((dp = readdir(dirp)) != NULL && !quit)
    {
        if (dp->d_type != DT_DIR) continue;
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) continue;
        
        char path[NAME_MAX+1];
        snprintf(path, NAME_MAX+1, "%s/%.*s", config->watch_dir, (int)dp->d_namlen, dp->d_name);
        
        struct stat s = {};
        int res = stat(path, &s);
        if (res != 0) {
            log_error("Failed to stat %s: %s", path, strerror(errno));
            continue;
        }
        
        // Check birth time to avoid re-processing
        struct timeval birthtimeval;
        TIMESPEC_TO_TIMEVAL(&birthtimeval, &s.st_birthtimespec);
        if (timercmp(&birthtimeval, &context->last_run, <)) {
            continue;  // Already processed
        }
        
        log_debug("Found new directory: %s", dp->d_name);
        
        band_info_t band_info;
        if (check_for_bandcamp_folder(path, dp->d_name, &band_info) != 0) {
            continue;  // Not a bandcamp folder
        }
        
        log_info("Found a folder that appears to be from bandcamp (%s)!", dp->d_name);
        
        // Get file extension string for lookup
        const char *ext_str = (band_info.file_type == flac) ? "flac" : 
                              (band_info.file_type == aac) ? "aac" : "unknown";
        
        // Find target directory
        const char *target_base = get_target_dir(config, ext_str);
        if (!target_base) {
            log_error("No target directory configured for %s files", ext_str);
            continue;
        }
        
        // Build destination paths
        char dst_path[NAME_MAX+1];
        char band_dst_path[NAME_MAX+1];
        snprintf(dst_path, NAME_MAX+1, "%s/%s/%s", target_base, band_info.name, band_info.album);
        snprintf(band_dst_path, NAME_MAX+1, "%s/%s", target_base, band_info.name);
        
        // Check if already exists
        if (dir_exists(dst_path)) {
            log_info("%s already exists, skipping...", dst_path);
            continue;
        }
        
        // Confirmation prompt if enabled
        if (config->confirm) {
            int confirm = confirm_action(dp->d_name, &band_info, path, dst_path, config->dry_run);
            if (confirm == -1) {
                quit = 1;
                break;
            } else if (confirm == 0) {
                if (skip_remaining) break;
                continue;
            }
        }
        
        // Create band directory if needed
        if (!dir_exists(band_dst_path)) {
            if (mkdir(band_dst_path, 0755)) {
                log_error("Failed to create directory %s: %s", band_dst_path, strerror(errno));
                continue;
            }
        }
        
        // Copy files (unless dry-run)
        if (config->dry_run) {
            log_info("[DRY RUN] Would copy files to %s", dst_path);
        } else {
            log_info("Copying files to %s", dst_path);
            if (clone(path, dst_path) != 0) {
                log_error("Failed to copy files to %s", dst_path);
                continue;
            }
        }
        
        // Add to Apple Music if enabled and format is supported
        if (config->apple_music && is_apple_music_format(ext_str)) {
            if (config->dry_run) {
                log_info("[DRY RUN] Would add %s to Apple Music", dst_path);
            } else {
                log_info("Adding %s to Apple Music", dst_path);
                add_folder_to_apple_music(dst_path);
            }
        }
    }
    
    (void)closedir(dirp);
    context->last_run.tv_sec = start_of_run.tv_sec;
    context->last_run.tv_usec = start_of_run.tv_usec;
    
    return quit ? -1 : 0;
}

int watch_folder(context_t *context)
{
    int kq;
    void *user_data = (void*)context;

    if ((kq = kqueue()) < 0) {
        log_error("Could not open kernel queue: %s", strerror(errno));
        return -1;
    }

    int dirfd = open(context->config->watch_dir, O_EVTONLY);
    if (dirfd <= 0) {
        log_error("Could not open %s for monitoring: %s", context->config->watch_dir, strerror(errno));
        return -1;
    }
   
    unsigned int vnode_events = NOTE_LINK;
    struct kevent direvent;
    EV_SET(&direvent, dirfd, EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE, vnode_events, 0, (void *)user_data);
    struct kevent event_data;

    while (1) {
        int event_count = kevent(kq, &direvent, 1, &event_data, 1, NULL);
        if ((event_count < 0) || (event_data.flags == EV_ERROR)) {
            log_error("kevent error: %s", strerror(errno));
            break;
        }
        if (event_count) {
            context_t *ctx = (context_t *)(event_data.udata);
            log_trace("Event occurred on %s", ctx->config->watch_dir);
            if (process(ctx) != 0) {
                break;  // Quit requested
            }
        }
    }
    
    close(kq);
    return 0;
}

int main(int argc, char *argv[])
{
    config_t config;
    
    // Step 1: Quick scan for help request (before any initialization)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return EXIT_SUCCESS;
        }
    }
    
    // Step 2: Initialize config with defaults
    config_init(&config);
    
    // Step 3: Find config file path from CLI (for config_load_file)
    char *cli_config_path = NULL;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--config") == 0) && i + 1 < argc) {
            cli_config_path = argv[i + 1];
            break;
        }
    }
    
    // Step 4: Load config file and apply defaults
    if (config_load_file(&config, cli_config_path) != 0) {
        fprintf(stderr, "Failed to load configuration file.\n");
        config_free(&config);
        return EXIT_FAILURE;
    }
    
    // Step 5: Parse CLI args (CLI overrides config file values)
    if (parse_args(&config, argc, argv) != 0) {
        fprintf(stderr, "Failed to parse arguments. Use -h for help.\n");
        config_free(&config);
        return EXIT_FAILURE;
    }
    
    // Step 6: Validate final configuration
    if (config_validate(&config) != 0) {
        fprintf(stderr, "Configuration validation failed. Use -h for help.\n");
        config_free(&config);
        return EXIT_FAILURE;
    }
    
    // Step 7: Set log level
    log_set_level(config.log_level);
    
    // Print configuration in debug mode
    if (config.log_level <= LOG_DEBUG) {
        config_print(&config);
    }
    
    // Check if watch directory exists
    if (!dir_exists(config.watch_dir)) {
        log_error("Watch directory does not exist: %s", config.watch_dir);
        config_free(&config);
        return EXIT_FAILURE;
    }
    
    // Initialize context
    context_t context = {
        .config = &config,
        .last_run = {0, 0}
    };
    
    log_info("Starting bandcamp_watcher");
    log_info("Watch directory: %s", config.watch_dir);
    if (config.oneshot) {
        log_info("Oneshot mode: processing once and exiting");
    }
    if (config.dry_run) {
        log_info("Dry-run mode: no changes will be made");
    }
    if (config.confirm) {
        log_info("Confirmation mode: will prompt for each album");
    }
    
    // Initial processing
    log_trace("Calling initial processing");
    process(&context);
    
    // Enter watch loop if not oneshot
    if (!config.oneshot) {
        log_info("Entering watch mode (press Ctrl+C to exit)");
        watch_folder(&context);
    }
    
    log_info("Exiting bandcamp_watcher");
    
    // Cleanup
    config_free(&config);
    return EXIT_SUCCESS;
}
