//
//  args.c
//  bandcamp_watcher
//
//  Command line argument parsing implementation
//

#include "args.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("Monitor downloads for Bandcamp albums and organize them by format.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -w, --watch DIR           Directory to watch (default: ~/Downloads)\n");
    printf("  -e, --ext EXT:DIR         Extension:directory mapping (repeatable)\n");
    printf("  -o, --oneshot             Run once and exit (no watching)\n");
    printf("  -c, --confirm             Prompt for confirmation before each action\n");
    printf("  -d, --dry-run             Simulate only, no actual changes\n");
    printf("  -A, --no-apple-music      Disable Apple Music integration\n");
    printf("  -f, --config FILE         Configuration file path\n");
    printf("  -v, --verbose             Enable verbose/debug logging\n");
    printf("  -q, --quiet               Suppress non-error output\n");
    printf("  -h, --help                Display this help and exit\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -w ~/Downloads -e flac:/Music/FLAC -e aac:/Music/AAC\n", program_name);
    printf("  %s --oneshot --confirm --dry-run\n", program_name);
    printf("  %s -v --no-apple-music\n", program_name);
}

void print_help(const char *program_name) {
    print_usage(program_name);
    printf("\n");
    printf("Configuration file:\n");
    printf("  Default: $XDG_CONFIG_HOME/bandcamp_watcher/config\n");
    printf("  Fallback: ~/.config/bandcamp_watcher/config\n");
    printf("\n");
    printf("Configuration file format (INI):\n");
    printf("  watch_dir = ~/Downloads\n");
    printf("  log_level = info\n");
    printf("  apple_music = true\n");
    printf("\n");
    printf("  [extensions]\n");
    printf("  flac = /Volumes/Music/FLAC\n");
    printf("  aac = /Volumes/Music/AAC\n");
    printf("\n");
    printf("Apple Music integration:\n");
    printf("  Enabled by default for: aac, m4a, mp3, alac\n");
    printf("  FLAC and other formats are not added to Apple Music\n");
    printf("\n");
    printf("Dry-run mode:\n");
    printf("  Shows what would be copied without making changes\n");
    printf("  Implies --oneshot (no directory watching)\n");
    printf("  Use with --verbose for detailed output\n");
}

int parse_args(config_t *config, int argc, char *argv[]) {
    if (!config) return -1;
    
    static struct option long_options[] = {
        {"watch", required_argument, 0, 'w'},
        {"ext", required_argument, 0, 'e'},
        {"oneshot", no_argument, 0, 'o'},
        {"confirm", no_argument, 0, 'c'},
        {"dry-run", no_argument, 0, 'd'},
        {"no-apple-music", no_argument, 0, 'A'},
        {"config", required_argument, 0, 'f'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    int option_index = 0;
    
    // Reset optind to allow multiple calls
    optind = 1;
    
    while ((c = getopt_long(argc, argv, "w:e:ocdAf:vqh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'w':
                if (config_set_watch_dir(config, optarg) != 0) {
                    return -1;
                }
                break;
                
            case 'e':
                if (config_add_mapping(config, optarg) != 0) {
                    return -1;
                }
                break;
                
            case 'o':
                config->oneshot = 1;
                break;
                
            case 'c':
                config->confirm = 1;
                break;
                
            case 'd':
                config->dry_run = 1;
                config->oneshot = 1;  // Dry-run implies oneshot
                break;
                
            case 'A':
                config->apple_music = 0;
                break;
                
            case 'f':
                // Config file path - already handled in config.c
                // We just consume it here
                break;
                
            case 'v':
                config->log_level = LOG_DEBUG;
                break;
                
            case 'q':
                config->log_level = LOG_ERROR;
                break;
                
            case 'h':
                print_help(argv[0]);
                return 1;
                
            case '?':
                // getopt_long already printed an error message
                return -1;
                
            default:
                return -1;
        }
    }
    
    // Check for remaining arguments (there shouldn't be any)
    if (optind < argc) {
        fprintf(stderr, "Error: Unexpected argument: %s\n", argv[optind]);
        return -1;
    }
    
    return 0;
}
