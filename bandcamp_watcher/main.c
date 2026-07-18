//
//  main.c
//  bandcamp_watcher
//
//  Created by Nickg on 18/6/2025.
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
#include <sys/wait.h>
#include <limits.h>
#include <spawn.h>

#include "log.h"         // logging functions
#include "copy.h"
#include "utils.h"
#include "bandcamp.h"
#include "folder.h"
#include "config.h"
#include "args.h"
#include "state_db.h"

typedef struct {
    config_t *config;
    struct timeval last_run;  // last time we processed an event
    state_db_t *state_db;
    struct timeval last_heartbeat;  // last time we wrote heartbeat
} context_t;

typedef enum {
    CONFIRM_PROCESS,
    CONFIRM_SKIP_ONE,
    CONFIRM_SKIP_ALL,
    CONFIRM_QUIT
} confirmation_result_t;

typedef enum {
    PROCESS_COMPLETED,
    PROCESS_QUIT,
    PROCESS_FATAL
} process_status_t;

typedef struct {
    process_status_t status;
    unsigned int error_count;
} process_result_t;

extern char **environ;

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
    if (flags & NOTE_REVOKE) {strcat(ret,or);strcat(ret,"NOTE_REVOKE");}
 
    return ret;
}

int add_folder_to_apple_music(const char *folder)
{
    if (!folder) return EINVAL;

    char *const argv[] = {
        "/usr/bin/osascript",
        "-e",
        "on run argv",
        "-e",
        "tell application \"Music\" to add POSIX file (item 1 of argv)",
        "-e",
        "end run",
        (char *)folder,
        NULL
    };
    pid_t pid;
    int result = posix_spawn(&pid, argv[0], NULL, NULL, argv, environ);
    if (result != 0) return result;

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return errno;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : EIO;
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
static confirmation_result_t confirm_action(const char *folder_name, const band_info_t *info,
                          const char *src_path, const char *dst_path,
                          int dry_run)
{
    printf("%sFound: %s (%s)\n", dry_run ? "[DRY RUN] " : "", folder_name,
           info->file_type);
    printf("  Source: %s\n", src_path);
    printf("  Destination: %s\n", dst_path);
    
    if (dry_run) {
        printf("  Would copy files\n");
        if (is_apple_music_format(info->file_type)) {
            printf("  Would add to Apple Music\n");
        } else {
            printf("  Would NOT add to Apple Music (format not supported)\n");
        }
        return CONFIRM_PROCESS;
    }
    
    printf("\nProcess this album? [y/n/s/q] ");
    fflush(stdout);
    
    char response[10];
    if (fgets(response, sizeof(response), stdin) == NULL) {
        return CONFIRM_QUIT;
    }
    
    // Remove newline
    response[strcspn(response, "\n")] = '\0';
    
    if (response[0] == 'y' || response[0] == 'Y') {
        return CONFIRM_PROCESS;
    } else if (response[0] == 'n' || response[0] == 'N') {
        return CONFIRM_SKIP_ONE;
    } else if (response[0] == 's' || response[0] == 'S') {
        return CONFIRM_SKIP_ALL;
    } else if (response[0] == 'q' || response[0] == 'Q') {
        return CONFIRM_QUIT;
    }
    
    // Default to yes if unclear
    return CONFIRM_PROCESS;
}

static process_result_t process(context_t *context)
{
    process_result_t result = { .status = PROCESS_COMPLETED, .error_count = 0 };
    config_t *config = context->config;
    if (!config) {
        result.status = PROCESS_FATAL;
        return result;
    }
    
    struct timeval start_of_run;
    gettimeofday(&start_of_run, NULL);
    
    // Update last_scan_at
    if (context->state_db) {
        state_db_set_last_scan(context->state_db);
    }
    
    DIR *dirp = opendir(config->watch_dir);
    struct dirent *dp = NULL;
    
    if (dirp == NULL) {
        log_error("Failed to open watch directory %s: %s", config->watch_dir, strerror(errno));
        result.status = PROCESS_FATAL;
        return result;
    }
    
    // Build extension list from config mappings
    const char **exts = malloc(config->num_mappings * sizeof(char*));
    if (!exts) {
        log_error("Out of memory building extension list");
        (void)closedir(dirp);
        result.status = PROCESS_FATAL;
        return result;
    }
    for (int i = 0; i < config->num_mappings; i++) {
        exts[i] = config->mappings[i].ext;
    }
    
    while ((dp = readdir(dirp)) != NULL)
    {
        if (dp->d_type != DT_DIR) continue;
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) continue;
        
        char path[PATH_MAX];
        if (path_join(path, sizeof(path), config->watch_dir, dp->d_name) != 0) {
            log_error("Source path is too long for %s", dp->d_name);
            result.error_count++;
            continue;
        }
        
        struct stat s = {};
        int res = stat(path, &s);
        if (res != 0) {
            log_error("Failed to stat %s: %s", path, strerror(errno));
            result.error_count++;
            continue;
        }
        
        // Check birth time to avoid re-processing
        struct timeval birthtimeval;
        TIMESPEC_TO_TIMEVAL(&birthtimeval, &s.st_birthtimespec);
        struct timeval modified;
        TIMESPEC_TO_TIMEVAL(&modified, &s.st_mtimespec);
        if (timercmp(&birthtimeval, &context->last_run, <) &&
            timercmp(&modified, &context->last_run, <)) {
            continue;  // Already processed
        }
        
        log_debug("Found new directory: %s", dp->d_name);
        
        band_info_t band_info = {0};  // Zero-initialize the struct
        int source_type;
        if (check_music_folder(path, dp->d_name, &band_info, exts, config->num_mappings, &source_type) != 0) {
            continue;  // Not a recognized music folder
        }
        
        log_debug("Band info after check: name='%s', album='%s', type='%s'", 
                  band_info.name, band_info.album, band_info.file_type);
        
        const char *source_name = (source_type == SOURCE_BANDCAMP) ? "Bandcamp" : 
                                  (source_type == SOURCE_QOBUZ) ? "Qobuz" : "Unknown";
        log_info("Found %s folder: %s (%s)", source_name, dp->d_name, band_info.file_type);
        
        // Find target directory using detected file type
        const char *target_base = get_target_dir(config, band_info.file_type);
        if (!target_base) {
            log_error("No target directory configured for %s files", band_info.file_type);
            result.error_count++;
            continue;
        }
        
        // Build destination paths
        char band_dst_path[PATH_MAX];
        char dst_path[PATH_MAX];
        if (path_join(band_dst_path, sizeof(band_dst_path), target_base, band_info.name) != 0 ||
            path_join(dst_path, sizeof(dst_path), band_dst_path, band_info.album) != 0) {
            log_error("Destination path is too long for %s", dp->d_name);
            result.error_count++;
            continue;
        }
        
        // Check if already exists
        if (dir_exists(dst_path)) {
            log_info("%s already exists, skipping...", dst_path);
            // Append skipped event
            if (context->state_db) {
                state_db_append_event(context->state_db, EVENT_ALBUM_SKIPPED,
                                      band_info.name, band_info.album,
                                      band_info.file_type,
                                      source_type == SOURCE_BANDCAMP ? SOURCE_TYPE_BANDCAMP : SOURCE_TYPE_QOBUZ,
                                      path, dst_path, "Album already exists", 0);
            }
            continue;
        }
        
        // Confirmation prompt if enabled
        if (config->confirm) {
            confirmation_result_t confirmation = confirm_action(dp->d_name, &band_info, path, dst_path, config->dry_run);
            if (confirmation == CONFIRM_QUIT) {
                result.status = PROCESS_QUIT;
                break;
            } else if (confirmation == CONFIRM_SKIP_ALL) {
                break;
            } else if (confirmation == CONFIRM_SKIP_ONE) {
                continue;
            }
        }
        
        // Create band directory if needed
        if (!config->dry_run && !dir_exists(band_dst_path)) {
            if (mkdir(band_dst_path, 0755)) {
                log_error("Failed to create directory %s: %s", band_dst_path, strerror(errno));
                result.error_count++;
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
                // Append failure event
                if (context->state_db) {
                    state_db_append_event(context->state_db, EVENT_COPY_FAILED,
                                          band_info.name, band_info.album,
                                          band_info.file_type, 
                                          source_type == SOURCE_BANDCAMP ? SOURCE_TYPE_BANDCAMP : SOURCE_TYPE_QOBUZ,
                                          path, dst_path, "Failed to copy files", -1);
                }
                result.error_count++;
                continue;
            }
            // Append success event
            if (context->state_db) {
                state_db_append_event(context->state_db, EVENT_ALBUM_COPIED,
                                      band_info.name, band_info.album,
                                      band_info.file_type,
                                      source_type == SOURCE_BANDCAMP ? SOURCE_TYPE_BANDCAMP : SOURCE_TYPE_QOBUZ,
                                      path, dst_path, NULL, 0);
            }
        }
        
        // Add to Apple Music if enabled and format is supported
        if (config->apple_music && is_apple_music_format(band_info.file_type)) {
            if (config->dry_run) {
                log_info("[DRY RUN] Would add %s to Apple Music", dst_path);
            } else {
                log_info("Adding %s to Apple Music", dst_path);
                int music_error = add_folder_to_apple_music(dst_path);
                if (music_error != 0) {
                    log_error("Failed to add %s to Apple Music: %s", dst_path, strerror(music_error));
                    if (context->state_db) {
                        state_db_append_event(context->state_db, EVENT_WATCHER_ERROR,
                                              band_info.name, band_info.album,
                                              band_info.file_type,
                                              source_type == SOURCE_BANDCAMP ? SOURCE_TYPE_BANDCAMP : SOURCE_TYPE_QOBUZ,
                                              path, dst_path, "Apple Music import failed", music_error);
                    }
                    result.error_count++;
                }
            }
        }
    }
    
    free(exts);
    (void)closedir(dirp);
    context->last_run.tv_sec = start_of_run.tv_sec;
    context->last_run.tv_usec = start_of_run.tv_usec;
    
    return result;
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
    if (dirfd < 0) {
        log_error("Could not open %s for monitoring: %s", context->config->watch_dir, strerror(errno));
        close(kq);
        return -1;
    }
   
    unsigned int vnode_events = NOTE_WRITE | NOTE_EXTEND | NOTE_LINK | NOTE_RENAME | NOTE_DELETE;
    struct kevent direvent;
    EV_SET(&direvent, dirfd, EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE, vnode_events, 0, (void *)user_data);
    struct kevent event_data;

    while (1) {
        struct timespec timeout = { .tv_sec = 30, .tv_nsec = 0 };
        int event_count = kevent(kq, &direvent, 1, &event_data, 1, &timeout);
        if (event_count < 0 || (event_count > 0 && event_data.flags == EV_ERROR)) {
            log_error("kevent error: %s", strerror(errno));
            break;
        }
        struct timeval now;
        gettimeofday(&now, NULL);
        if (context->state_db) {
            struct timeval diff;
            timersub(&now, &context->last_heartbeat, &diff);
            if (diff.tv_sec >= 30) {
                state_db_heartbeat(context->state_db);
                context->last_heartbeat = now;
            }
        }
        if (event_count > 0) {
            context_t *ctx = (context_t *)(event_data.udata);
            log_trace("Event occurred on %s", ctx->config->watch_dir);
            process_result_t process_result = process(ctx);
            if (process_result.status == PROCESS_QUIT) {
                close(dirfd);
                close(kq);
                return 0;
            }
            if (process_result.status == PROCESS_FATAL) {
                close(dirfd);
                close(kq);
                return -1;
            }
        }
    }
    
    close(dirfd);
    close(kq);
    return -1;
}

#ifndef BCW_PROCESSOR_TESTS
int main(int argc, char *argv[])
{
    config_t config;
    int exit_status = EXIT_SUCCESS;
    
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
    
    // Initialize state database
    state_db_t *state_db = NULL;
    if (state_db_open(NULL, &state_db) == 0) {
        int pid = getpid();
        runtime_mode_t mode = config.oneshot ? RUNTIME_MODE_ONESHOT : RUNTIME_MODE_WATCH;
        state_db_init_runtime(state_db, mode, pid);
        state_db_set_runtime_status(state_db, RUNTIME_STATUS_STARTING, NULL);
        state_db_append_event(state_db, EVENT_WATCHER_STARTED, NULL, NULL, NULL, 0, NULL, NULL, NULL, 0);
        state_db_set_runtime_status(state_db, RUNTIME_STATUS_RUNNING, NULL);
        log_debug("State database initialized at %s", state_db_get_path(state_db));
    } else {
        log_warn("Failed to initialize state database, continuing without state tracking");
    }
    
    // Initialize context
    context_t context = {
        .config = &config,
        .last_run = {0, 0},
        .state_db = state_db,
        .last_heartbeat = {0, 0}
    };
    if (state_db) {
        gettimeofday(&context.last_heartbeat, NULL);
    }
    
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
    process_result_t initial_result = process(&context);
    if (initial_result.status == PROCESS_FATAL ||
        (config.oneshot && initial_result.error_count > 0)) {
        exit_status = EXIT_FAILURE;
    }
    
    // Enter watch loop if not oneshot
    if (!config.oneshot && initial_result.status != PROCESS_QUIT &&
        initial_result.status != PROCESS_FATAL) {
        log_info("Entering watch mode (press Ctrl+C to exit)");
        if (watch_folder(&context) != 0) {
            exit_status = EXIT_FAILURE;
        }
    }
    
    log_info("Exiting bandcamp_watcher");
    
    // Shutdown state database
    if (context.state_db) {
        if (config.oneshot && exit_status == EXIT_SUCCESS) {
            state_db_append_event(context.state_db, EVENT_ONESHOT_COMPLETED, NULL, NULL, NULL, 0, NULL, NULL, NULL, 0);
        }
        if (exit_status != EXIT_SUCCESS) {
            state_db_append_event(context.state_db, EVENT_WATCHER_ERROR, NULL, NULL, NULL, 0,
                                  NULL, NULL, "Watcher exited after an error", exit_status);
            state_db_set_runtime_status(context.state_db, RUNTIME_STATUS_ERROR,
                                        "Watcher exited after an error");
        }
        state_db_append_event(context.state_db, EVENT_WATCHER_STOPPED, NULL, NULL, NULL, 0, NULL, NULL, NULL, 0);
        if (exit_status == EXIT_SUCCESS) {
            state_db_set_runtime_status(context.state_db, RUNTIME_STATUS_STOPPED, NULL);
        }
        state_db_close(context.state_db);
    }
    
    // Cleanup
    config_free(&config);
    return exit_status;
}
#endif
