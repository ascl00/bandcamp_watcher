Recommended defaults in this plan
- DB path: ~/.local/state/bandcamp_watcher/state.sqlite3 (XDG-style)
- Launchd label: launched.bandcamp_watcher
- UI target: Objective-C macOS app (Bandcamp Watcher Status) in same Xcode project
1) Architecture
- Daemon (bandcamp_watcher) remains primary source of truth
  - Watches folders, copies music, optional Apple Music add
  - Writes operational events/state to SQLite
- Status app is read-only observer
  - Queries launchd/SMAppService for service status
  - Reads SQLite for recent albums and runtime metadata
- No hard dependency from daemon to UI
  - If UI never runs, daemon still works normally
  - If daemon not running, UI still launches and reports state
2) SQLite Data Model
Use a small schema with explicit versioning.
meta
- key TEXT PRIMARY KEY
- value TEXT NOT NULL
- Store schema_version, app version, etc.
runtime
Single-row current process/running snapshot.
- id INTEGER PRIMARY KEY CHECK (id = 1)
- pid INTEGER
- mode TEXT NOT NULL (watch, oneshot)
- status TEXT NOT NULL (starting, running, idle, stopping, stopped, error)
- started_at INTEGER (unix epoch)
- heartbeat_at INTEGER (unix epoch)
- last_error TEXT
- last_scan_at INTEGER
events
Append-only history.
- id INTEGER PRIMARY KEY AUTOINCREMENT
- ts INTEGER NOT NULL
- type TEXT NOT NULL
  - e.g. watcher_started, watcher_stopped, album_copied, album_skipped, copy_failed, oneshot_completed
- artist TEXT
- album TEXT
- file_type TEXT
- source_type INTEGER (1=Bandcamp, 2=Qobuz)
- src_path TEXT
- dest_path TEXT
- message TEXT
- exit_code INTEGER
Indexes:
- CREATE INDEX idx_events_ts ON events(ts DESC);
- CREATE INDEX idx_events_type_ts ON events(type, ts DESC);
SQLite settings on open:
- PRAGMA journal_mode=WAL;
- PRAGMA synchronous=NORMAL;
- PRAGMA busy_timeout=2000;
- PRAGMA user_version=1;
3) Daemon Changes (C)
Add a new module:
- lib/state_db.c
- lib/state_db.h
Public API (proposed)
- int state_db_open(const config_t *config, sqlite3 **db_out);
- int state_db_init_schema(sqlite3 *db);
- int state_db_update_runtime(sqlite3 *db, ...);
- int state_db_append_event(sqlite3 *db, ...);
- int state_db_trim(sqlite3 *db, int max_events);
- void state_db_close(sqlite3 *db);
Integration points in main.c
- On startup:
  - open/init DB
  - set runtime status starting -> running
  - append watcher_started event
- On each processing cycle:
  - update heartbeat_at and last_scan_at
- On successful album copy:
  - append album_copied with artist/album/file_type/source/dest
- On failures:
  - append copy_failed + message
- On clean shutdown (SIGINT, SIGTERM, normal exit):
  - set runtime stopping -> stopped
  - append watcher_stopped
- In --oneshot:
  - mode=oneshot
  - append start + completion event
  - update runtime appropriately before exit
Important behavior details
- If DB write fails, daemon continues (log warning; do not fail copy pipeline)
- Keep retention bounded:
  - e.g. keep latest 5,000 events or 30 days (pick one)
- Optional env override for debugging:
  - BCW_STATE_DB=/tmp/bcw.sqlite3
4) Status App (Objective-C) Plan
You’ll create the target manually; implementation can assume target exists.
Core classes
- BCWStatusController
  - owns NSStatusItem, menu rebuild, timers
- BCWServiceMonitor
  - queries launchd/SMAppService status
- BCWStateStore
  - read-only SQLite queries using sqlite3
- BCWConfigReader
  - reads config file path and displays contents/summary
Status strategy
- Primary: launchd status (SMAppService where suitable)
- Supplemental: runtime row from DB for heartbeat + mode + last error
- Display should clearly separate:
  - “Service Enabled/Loaded”
  - “Last heartbeat Xs ago”
  - “Last copied album ...”
Menu content (initial)
- Bandcamp Watcher: Running/Stopped
- Mode: watch|oneshot
- Last heartbeat: 12s ago
- Separator
- Recent Albums (last 10 album_copied)
- Separator
- Open Config
- Open State DB Folder
- Refresh Now
- Quit
Refresh cadence:
- Every 5s (simple polling), and on menu open do immediate refresh
5) Service Status Query Plan
Because SMAppService can be limited for arbitrary existing labels, use an abstraction:
- BCWServiceMonitor
  - try SMAppService status first
  - fallback to launchctl print gui/<uid>/launched.bandcamp_watcher parse
  - return normalized enum:
    - unknown, enabled_not_running, running, disabled, error
This keeps the UI future-proof if you later move fully to SMAppService registration.
6) Config Display (Read-only)
- Resolve config using same logic as daemon:
  - $XDG_CONFIG_HOME/bandcamp_watcher/config
  - fallback ~/.config/bandcamp_watcher/config
- Show in UI:
  - watch dir
  - extension mappings
  - apple_music on/off
  - log level
- Provide “Open in Editor” action only (no in-app edits yet)
7) Migration and Compatibility
- Existing users: no migration needed
- On first run:
  - daemon auto-creates DB + schema
- UI behavior when DB missing:
  - show “No state yet; daemon has not written status”
- Works with both watch mode and oneshot mode automatically
8) Testing Plan
Daemon tests
- Unit tests for state_db:
  - schema creation
  - append/read event correctness
  - runtime updates
  - retention trim
- Integration:
  - oneshot run writes start/stop + copied events
  - watch-mode startup writes runtime + heartbeat updates
UI tests/manual checks
- App launches with daemon stopped -> shows stopped state
- Start daemon -> status transitions correctly
- Copy an album -> recent list updates
- DB temporarily unavailable -> UI degrades gracefully
9) Rollout Phases
Phase 1: State backend in daemon
- Add state_db module + schema + writes
- Keep existing stderr logging unchanged
- Verify all current tests still pass
Phase 2: Objective-C status target
- Add menubar UI skeleton + periodic refresh
- Show service status + runtime row
Phase 3: Recent albums + config view
- Add event queries and menu population
- Add open-config/open-db actions
Phase 4: polish
- Retention tuning
- Better status text and timestamps
- Error surfacing in UI
10) Decisions You May Want to Confirm Before Implementation
1. Retention policy: max 5000 events vs 30 days
2. DB location preference:
   - keep XDG state path (recommended)
   - or move to ~/Library/Application Support/bandcamp_watcher/
3. Whether to include album_skipped events or only successful copies/errors
If you want, next I can convert this into a concrete task checklist ordered by file and function so implementation is mechanical.
