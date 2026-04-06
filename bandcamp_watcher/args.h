//
//  args.h
//  bandcamp_watcher
//
//  Command line argument parsing
//

#ifndef ARGS_H
#define ARGS_H

#include "lib/config.h"

// Parse command line arguments into config
// Already initialized with defaults and config file values
// Returns 0 on success, -1 on error, 1 if help was requested
int parse_args(config_t *config, int argc, char *argv[]);

// Print usage information
void print_usage(const char *program_name);

// Print help information
void print_help(const char *program_name);

#endif /* ARGS_H */
