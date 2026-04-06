# bandcamp_watcher

Automatically organize Bandcamp downloads by audio format and add them to your music library.

A few things. Firstly, I wrote this for my own use, as a MacOS version of BandcampExpand also in this repo. BandcampExpand was immproved on in a few ways, but the main one is that with MacOS I can use kqueue's to monitor for changes, and automatically process them. This is written in C, yes, plain C. The kqueue interface is a C interface, and I possibly should have written this in Rust or Zig and figured out how to interoperate with kqueues, but I needed something and C was the shortest path for me.


## Overview

`bandcamp_watcher` monitors your Downloads folder (or a specified directory) for new Bandcamp album downloads. When it detects a Bandcamp folder structure, it:

1. Identifies the album by parsing the folder name (`Artist - Album`)
2. Determines the audio format (FLAC, AAC/M4A, etc.)
3. Copies the files to your organized music library structure
4. Optionally adds supported formats to Apple Music

## Features

- **Automatic detection**: Recognizes Bandcamp download folder structure
- **Format organization**: Sorts FLAC, AAC, M4A, MP3, and ALAC into separate directories
- **Apple Music integration**: Automatically adds supported formats (not FLAC) to Apple Music
- **Flexible configuration**: Command-line arguments or config file
- **Dry-run mode**: Preview what would happen without making changes
- **Confirmation mode**: Review each album before processing

## Building

### Requirements
- macOS (uses kqueue for file system monitoring)
- Xcode or command line tools

### Build with Xcode
1. Open `bandcamp_watcher.xcodeproj`
2. Select the `bandcamp_watcher` scheme
3. Build (⌘B) or Run (⌘R)

### Build from command line
```bash
cd bandcamp_watcher.xcodeproj
xcodebuild -scheme bandcamp_watcher -configuration Release
```

## Configuration

### Configuration File

Default location: `$XDG_CONFIG_HOME/bandcamp_watcher/config`
(or `~/.config/bandcamp_watcher/config` if XDG_CONFIG_HOME is not set)

Create the directory and file:
```bash
mkdir -p ~/.config/bandcamp_watcher
```

### Sample Configuration

```ini
# Global settings
watch_dir = ~/Downloads
log_level = info
apple_music = true

# Extension mappings [extensions]
# Format: extension = /path/to/destination
[extensions]
flac = /Volumes/Multimedia/Music/FLAC
aac = /Volumes/Multimedia/Music/aac
m4a = /Volumes/Multimedia/Music/aac
mp3 = /Volumes/Multimedia/Music/mp3
alac = /Volumes/Multimedia/Music/alac
```

### Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `watch_dir` | Directory to monitor for new downloads | `~/Downloads` |
| `log_level` | Logging verbosity (debug, trace, info, warn, error, fatal) | `info` |
| `apple_music` | Add supported formats to Apple Music | `true` |

### Extension Mappings

Specify where each audio format should be copied in the `[extensions]` section:

```ini
[extensions]
flac = /path/to/flac/library
aac = /path/to/aac/library
m4a = /path/to/aac/library  ; M4A and AAC can share a location
```

**Note**: FLAC files are NOT added to Apple Music (Apple Music doesn't support FLAC). All other formats (AAC, M4A, MP3, ALAC) are added if `apple_music` is enabled.

## Command Line Usage

```
bandcamp_watcher [OPTIONS]
```

### Options

| Option | Description |
|--------|-------------|
| `-w, --watch DIR` | Directory to watch (default: ~/Downloads) |
| `-e, --ext EXT:DIR` | Extension:directory mapping pair (repeatable) |
| `-o, --oneshot` | Run once and exit (no watching) |
| `-c, --confirm` | Prompt for confirmation before each action |
| `-d, --dry-run` | Simulate only, no actual changes |
| `-A, --no-apple-music` | Disable Apple Music integration |
| `-f, --config FILE` | Configuration file path |
| `-v, --verbose` | Enable verbose/debug logging |
| `-q, --quiet` | Suppress non-error output |
| `-h, --help` | Display help and exit |

### Examples

**Watch Downloads with default settings:**
```bash
./bandcamp_watcher
```

**Specify custom directories:**
```bash
./bandcamp_watcher -w ~/Downloads \
    -e flac:/Volumes/Music/FLAC \
    -e aac:/Volumes/Music/AAC
```

**Dry-run to preview what would happen:**
```bash
./bandcamp_watcher --dry-run --verbose
```

**Process once with confirmation:**
```bash
./bandcamp_watcher --oneshot --confirm
```

**Use custom config file:**
```bash
./bandcamp_watcher -f ~/.config/my_bcw_config
```

## Modes of Operation

### Watch Mode (Default)
Continuously monitors the watch directory for new folders:
```bash
./bandcamp_watcher
```
Press `Ctrl+C` to exit.

### Oneshot Mode
Process existing folders once and exit:
```bash
./bandcamp_watcher --oneshot
```

### Dry-Run Mode
Shows what would be copied without making changes (implies oneshot):
```bash
./bandcamp_watcher --dry-run --verbose
```

### Confirmation Mode
Prompts before processing each album (useful with oneshot):
```bash
./bandcamp_watcher --oneshot --confirm
```

Prompt format:
```
Found: Artist - Album (flac)
  Source: /Users/you/Downloads/Artist - Album
  Destination: /Volumes/Music/FLAC/Artist/Album

Process this album? [y/n/s/q]
```
- `y` - Process this folder
- `n` - Skip this folder
- `s` - Skip remaining folders
- `q` - Quit immediately

## Testing

Run unit tests in Xcode:
1. Select the `bandcamp_watcherTests` scheme
2. Run tests with ⌘U

Or from command line:
```bash
xcodebuild test -scheme bandcamp_watcherTests
```

### Test Coverage

- **StringUtilsTests** (29 tests): String comparison, extension matching, trimming, boolean parsing
- **FileOpsTests** (11 tests): Directory operations, file copying
- **BandcampTests** (10 tests): Folder detection and validation
- **LogTests** (9 tests): Log level parsing
- **ConfigTests** (22 tests): Configuration loading, validation, XDG paths

## Running as a Service (launchd)

To run bandcamp_watcher automatically in the background on macOS, use the included `bandcamp_watcher.plist` file with launchd.

### Setup

1. **Build the binary** and place it in a permanent location (e.g., `~/bin/` or `/usr/local/bin/`):
   ```bash
   mkdir -p ~/bin
   cp /path/to/built/bandcamp_watcher ~/bin/
   ```

2. **Copy the plist file** to your LaunchAgents directory:
   ```bash
   cp bandcamp_watcher/bandcamp_watcher.plist ~/Library/LaunchAgents/
   ```

3. **Edit the plist file** to match your system:
   - Replace `USERNAME` with your macOS username throughout the file
   - Update the path in `ProgramArguments` if you placed the binary elsewhere
   - Optional: Add command-line arguments by adding more `<string>` elements to `ProgramArguments`

4. **Load the service**:
   ```bash
   launchctl load ~/Library/LaunchAgents/bandcamp_watcher.plist
   ```

5. **Start the service immediately** (or wait for next login):
   ```bash
   launchctl start launched.bandcamp_watcher
   ```

### Managing the Service

| Command | Description |
|---------|-------------|
| `launchctl start launched.bandcamp_watcher` | Start the service |
| `launchctl stop launched.bandcamp_watcher` | Stop the service |
| `launchctl unload ~/Library/LaunchAgents/bandcamp_watcher.plist` | Unload (disable) the service |
| `launchctl list | grep bandcamp_watcher` | Check if service is loaded |

### Viewing Logs

Logs are written to the system log, viewable via Console.app or:
```bash
log show --predicate 'process == "bandcamp_watcher"' --last 1h
```

Or for real-time logs:
```bash
log stream --predicate 'process == "bandcamp_watcher"'
```

## Troubleshooting

### "Watch directory does not exist"
The directory specified with `-w` or in the config file doesn't exist. Create it or specify a different path.

### "No extension mappings specified"
You need to define at least one extension mapping in the config file or via `-e` arguments.

### Albums not being detected
Ensure the downloaded folder follows Bandcamp's naming convention:
- Folder name: `Artist Name - Album Name`
- Song files: `Artist Name - Album Name - 01 Song Name.ext`

## License

MIT License - See LICENSE file for details.
