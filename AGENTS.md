# AGENTS

## Repo facts that matter
- This is a single Xcode project: `bandcamp_watcher.xcodeproj`.
- Real targets/schemes are `bandcamp_watcher` (CLI), `watcherlib` (static C library), `tests` (XCTest bundle), and `Bandcamp Watcher Status` (macOS menu bar app).
- The `tests` target links `watcherlib`; most C logic lives under `bandcamp_watcher/lib/` and is exercised from Objective-C tests in `tests/*.m`.

## Verified commands (prefer these)
- List project targets/schemes: `xcodebuild -list -project "bandcamp_watcher.xcodeproj"`
- Build CLI: `xcodebuild -project "bandcamp_watcher.xcodeproj" -scheme bandcamp_watcher -configuration Debug build`
- Build status app: `xcodebuild -project "bandcamp_watcher.xcodeproj" -scheme "Bandcamp Watcher Status" -configuration Debug build`
- Run library/unit tests: `xcodebuild test -project "bandcamp_watcher.xcodeproj" -scheme tests -destination "platform=macOS"`
- Run one test class: `xcodebuild test -project "bandcamp_watcher.xcodeproj" -scheme tests -destination "platform=macOS" -only-testing:tests/StringUtilsTests`

## Config + runtime paths (from code)
- Config file lookup is `"$XDG_CONFIG_HOME/bandcamp_watcher/config"` then `"~/.config/bandcamp_watcher/config"` (`bandcamp_watcher/lib/config.c`).
- State DB lookup is `"$XDG_STATE_HOME/bandcamp_watcher/state.sqlite3"` then `"~/.local/state/bandcamp_watcher/state.sqlite3"` (`bandcamp_watcher/lib/state_db.c`).
- Launchd label expected by the status app and plist is `launched.bandcamp_watcher` (`bandcamp_watcher/bandcamp_watcher.plist`, `Bandcamp Watcher Status/BCWServiceMonitor.m`).

## Easy-to-miss behavior
- `--dry-run` forces oneshot mode (`config->oneshot = 1` in `bandcamp_watcher/args.c`); do not expect watcher loop behavior in dry runs.
- `watch_dir` must exist or config loading fails (`config_set_watch_dir` validates path).
- If no extension mappings are provided, defaults come from code (not README): `flac`, `aac`, `mp3` only (`bandcamp_watcher/lib/config.c`).
- Status app now shows a recent-copy dot (`•`) for unseen `album_copied` events and clears it when the menu opens (`Bandcamp Watcher Status/BCWStatusController.m`).
- Status app intentionally does not show a stale-heartbeat warning; idle for long periods is treated as normal (`Bandcamp Watcher Status/BCWStatusController.m`).
- Status app menu exposes service controls by state: Start when not running, Restart/Stop when running; controls call `launchctl` via `BCWServiceMonitor` (`Bandcamp Watcher Status/BCWServiceMonitor.m`, `Bandcamp Watcher Status/BCWStatusController.m`).

## Docs vs code
- README test instructions mention `bandcamp_watcherTests`, but executable source of truth is the `tests` scheme/target.
