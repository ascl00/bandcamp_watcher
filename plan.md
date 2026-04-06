# Implementation Plan: Command Line Arguments and Configuration

## Overview
Add comprehensive command line argument parsing and configuration file support to bandcamp_watcher, enabling flexible runtime configuration without recompilation.

## Goals
1. Parse command line arguments using getopt_long
2. Support XDG-compliant configuration files
3. Add oneshot mode with dry-run and confirmation options
4. Make Apple Music integration toggleable (enabled by default)
5. Support variable-length extension-to-directory mappings

## New Files to Create

### lib/config.h
- Define `log_level_t` typedef (moved from log.h enum)
- Define `ext_mapping_t` struct for extension->directory pairs
- Define `config_t` struct holding all configuration
- Function declarations:
  - `int config_load(config_t *config, int argc, char *argv[])`
  - `void config_free(config_t *config)`
  - `void config_print(const config_t *config)`
  - `char *get_xdg_config_path(void)`

### lib/config.c
- XDG Base Directory specification implementation
- INI-style config file parsing
- Default value initialization
- Configuration priority: CLI args > config file > defaults

### lib/args.h
- Function declarations for CLI parsing
- `int parse_args(config_t *config, int argc, char *argv[])`

### lib/args.c
- getopt_long implementation
- Support for:
  - `-w, --watch DIR`
  - `-e, --ext EXT:DIR` (repeatable)
  - `-o, --oneshot`
  - `-c, --confirm`
  - `-d, --dry-run`
  - `--no-apple-music`
  - `-f, --config FILE`
  - `-v, --verbose`
  - `-q, --quiet`
  - `-h, --help`

## Modified Files

### lib/log.h
Add typedef for log level enum:
```c
typedef enum {
    LOG_DEBUG = -32,
    LOG_TRACE = 0,
    LOG_INFO = 32,
    LOG_WARN = 64,
    LOG_ERROR = 96,
    LOG_FATAL = 128,
} log_level_t;
```

### bandcamp_watcher/main.c
- Replace hardcoded paths with config system
- Add oneshot mode logic (run once, exit)
- Add dry-run mode (simulate without changes)
- Add confirmation prompts
- Integrate Apple Music whitelist check

### tests/ConfigTests.m (new)
- Test config file parsing
- Test CLI argument parsing
- Test priority ordering (CLI > config > defaults)
- Test XDG path resolution

## Data Structures

```c
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
```

## Configuration Priority
1. Command line arguments (highest)
2. Configuration file values
3. Built-in defaults (lowest)

## Configuration File Format

Path: `$XDG_CONFIG_HOME/bandcamp_watcher/config`
(fallback: `~/.config/bandcamp_watcher/config`)

Format: INI-style
```ini
# Global settings
watch_dir = ~/Downloads
log_level = info
apple_music = true

# Extension mappings [extensions]
flac = /Volumes/Multimedia/Music/FLAC
aac = /Volumes/Multimedia/Music/aac
m4a = /Volumes/Multimedia/Music/aac
```

## Apple Music Integration

**Supported formats (whitelist):**
- aac
- m4a
- mp3
- alac

**Not supported:**
- flac

When `apple_music` is enabled and a matching folder contains files in supported formats, the destination folder will be added to Apple Music after copying.

## Command Line Interface

```
bandcamp_watcher [OPTIONS]

Options:
  -w, --watch DIR           Directory to watch (default: ~/Downloads)
  -e, --ext EXT:DIR         Extension:directory mapping pair (repeatable)
                           Example: -e flac:/path/to/flac -e aac:/path/to/aac
  -o, --oneshot             Run once and exit (no watching)
  -c, --confirm             Prompt for confirmation before each action
  -d, --dry-run             Simulate only, no actual changes (implies oneshot)
  -A, --no-apple-music      Disable Apple Music integration
  -f, --config FILE         Configuration file path
  -v, --verbose             Enable verbose/debug logging
  -q, --quiet               Suppress non-error output
  -h, --help                Display help and exit
```

## Oneshot Mode with Confirmation

When both `--oneshot` and `--confirm` are specified:

```
Found: Band - Album (flac)
  Source: /Users/nick/Downloads/Band - Album
  Destination: /Volumes/Music/FLAC/Band/Album
  Files: 12 tracks

Process this album? [y/n/s/q]
```

- `y` - Process this folder
- `n` - Skip this folder
- `s` - Skip remaining folders
- `q` - Quit immediately

## Dry-Run Mode

When `--dry-run` is specified:
- Implies `--oneshot` (no watching)
- Logs actions via `log_info()` but doesn't execute them
- Shows what would be copied and where
- Indicates whether Apple Music would be notified

Example output:
```
[DRY RUN] Found: Band - Album (flac)
  Source: /Users/nick/Downloads/Band - Album
  Destination: /Volumes/Music/FLAC/Band/Album
  Would copy 12 files
  Would NOT add to Apple Music (flac not in supported formats)
```

## Default Values

| Setting | Default Value |
|---------|---------------|
| watch_dir | $HOME/Downloads |
| log_level | LOG_INFO |
| apple_music | true (1) |
| oneshot | false (0) |
| confirm | false (0) |
| dry_run | false (0) |

## Implementation Notes

### Extension Mapping Parsing
- CLI format: `-e flac:/path/to/dir`
- Use `strchr()` to find colon separator
- Support multiple `-e` arguments via dynamic array (realloc)
- Config file: one line per mapping in [extensions] section

### XDG Path Resolution
1. Check `$XDG_CONFIG_HOME`
2. If unset, use `$HOME/.config`
3. Append `/bandcamp_watcher/config`

### Log Level Mapping
- CLI `-v` sets LOG_DEBUG
- CLI `-q` sets LOG_ERROR
- Config file uses string names: "debug", "trace", "info", "warn", "error", "fatal"
- Case-insensitive matching

### Memory Management
- All strings in config_t must be heap-allocated (strdup)
- config_free() must release all allocated memory
- Handle parse errors gracefully (log error, use default)

## Testing Plan

### Unit Tests
1. **Config file parsing**
   - Valid config with all options
   - Missing optional values (use defaults)
   - Invalid log level string
   - Malformed extension mapping
   - Missing config file (should not error)

2. **CLI argument parsing**
   - Single extension mapping
   - Multiple extension mappings
   - All boolean flags
   - Missing required arguments
   - Help display

3. **Priority ordering**
   - CLI overrides config file
   - Config file overrides defaults
   - Multiple CLI args (last wins)

4. **XDG path resolution**
   - $XDG_CONFIG_HOME set
   - $XDG_CONFIG_HOME unset
   - $HOME unset (edge case)

### Integration Tests
1. Dry-run mode output verification
2. Confirmation prompt handling
3. Apple Music whitelist check

## Build Integration

- Add `lib/config.c` and `lib/args.c` to static library target
- Add headers to public headers list
- Update tests target with new test files

## Future Considerations

- Signal handling for graceful shutdown in watch mode
- Config file auto-reload on change
- Plugin system for additional music services
