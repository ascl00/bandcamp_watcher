Phase 0: Defaults (lock these in first)
- DB path: $XDG_STATE_HOME/bandcamp_watcher/state.sqlite3, fallback ~/.local/state/bandcamp_watcher/state.sqlite3
- Launchd label: launched.bandcamp_watcher
- Retention: keep latest 5000 events
- UI refresh: every 5s + immediate refresh when menu opens
---
Phase 1: Daemon State Backend (SQLite)
1) Add bandcamp_watcher/lib/state_db.h (new)
- Define event/runtime APIs:
  - state_db_open(...)
  - state_db_init_schema(...)
  - state_db_set_runtime(...)
  - state_db_heartbeat(...)
  - state_db_append_event(...)
  - state_db_trim(...)
  - state_db_close(...)
- Define event type constants:
  - watcher_started, watcher_stopped, album_copied, album_skipped, copy_failed, oneshot_completed
2) Add bandcamp_watcher/lib/state_db.c (new)
Implement:
- Path resolver
  - Resolve XDG state path + fallback
  - Ensure parent directories exist
- SQLite open/setup
  - WAL, busy_timeout, synchronous=NORMAL
- Schema bootstrap
  - meta, runtime, events
  - indexes on events(ts DESC) and (type, ts DESC)
- Runtime upsert
  - Single-row runtime (id=1)
- Event inserts
  - Prepared statement insert for append-only events
- Retention
  - Delete old rows beyond max limit
3) Update bandcamp_watcher/lib/state_db.c schema/functions
- meta(key,value) includes schema_version=1
- runtime(id,pid,mode,status,started_at,heartbeat_at,last_error,last_scan_at)
- events(id,ts,type,artist,album,file_type,source_type,src_path,dest_path,message,exit_code)
---
Phase 2: Hook Daemon Runtime + Events
4) Update bandcamp_watcher/main.c
- Include state_db.h
- Extend context_t:
  - sqlite3 *state_db
  - optional last heartbeat timestamp for throttling
- Startup flow
  - open/init DB
  - runtime: starting -> running
  - append watcher_started
- Oneshot flow
  - set mode=oneshot
  - append oneshot_completed at end with exit status
- Watch mode loop
  - heartbeat update every cycle (or every 10–30s)
  - update last_scan_at
- Album processing hooks
  - on successful copy: append album_copied (artist/album/type/source/src/dest)
  - on skip/failure: append album_skipped or copy_failed (+message)
- Shutdown
  - on clean exit/signal: append watcher_stopped, set runtime stopped, close DB
- Failure policy
  - DB write failures must never abort core watcher flow; just log warning
5) Optional config knobs (defer if you want minimal first cut)
- bandcamp_watcher/lib/config.h + config.c:
  - state_db_path override
  - max_state_events
- If deferred: use compiled defaults above
---
Phase 3: Tests
6) Add Bandcamp Watcher StatusTests/StateDbTests.m (new)
- testCreatesSchema
- testUpsertsRuntimeRow
- testAppendsAlbumEvent
- testRetentionTrim
- testOpenUsesFallbackPath
7) Update integration tests (if needed)
- Bandcamp Watcher StatusTests/ValidateTests.m or daemon-level tests:
  - verify album copy path triggers album_copied event
  - verify oneshot writes start/completed/stopped sequence
---
Phase 4: Objective-C Status App Target (you create target manually)
8) New target: Bandcamp Watcher Status
- macOS App (Objective-C)
- LSUIElement=1 (menu bar only)
- Frameworks:
  - AppKit
  - ServiceManagement
  - SQLite3
9) Add Bandcamp Watcher Status/BCWServiceMonitor.h/.m (new)
- API:
  - - (BCWServiceState)currentState
- Implementation:
  - try SMAppService status where usable
  - fallback launchctl print gui/<uid>/launched.bandcamp_watcher
- Normalize to:
  - running, enabled_not_running, disabled, unknown, error
10) Add Bandcamp Watcher Status/BCWStateStore.h/.m (new)
- Read-only sqlite access
- API:
  - - (nullable BCWRuntimeStatus *)fetchRuntime
  - - (NSArray<BCWAlbumEvent *> *)fetchRecentAlbums:(NSUInteger)limit
- Queries:
  - runtime row by id=1
  - recent events where type='album_copied' order by ts desc limit 10
11) Add Bandcamp Watcher Status/BCWStatusController.h/.m (new)
- Owns NSStatusItem
- Timer refresh every 5s
- Menu sections:
  - service state
  - mode + heartbeat age
  - recent albums (last 10)
  - actions: Refresh Now, Open Config, Open DB Folder, Quit
12) Add Bandcamp Watcher Status/AppDelegate.m (or scene entry)
- Initialize BCWStatusController
- start refresh loop
---
Phase 5: UX + Reliability
13) Add stale/health logic in UI
- If heartbeat_at older than threshold (e.g. 60s), show “stale”
- If launchd says enabled but no heartbeat, show “enabled, not reporting”
14) Add graceful empty states
- No DB yet: “No state written yet”
- No recent albums: “No recent copies”
---
Phase 6: Acceptance Criteria
- bandcamp_watcher --oneshot still logs to stderr and completes normally
- Daemon writes runtime + events without requiring UI
- UI shows:
  - launchd/service state
  - last heartbeat
  - recent copied albums
- UI works if daemon is stopped
- All existing tests pass + new StateDbTests pass
