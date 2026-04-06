Commit Plan
1) state-db: add sqlite runtime/event backend
Goal: introduce storage layer without touching watcher behavior yet.
- Add bandcamp_watcher/lib/state_db.h
- Add bandcamp_watcher/lib/state_db.c
- Implement:
  - DB path resolution (XDG_STATE_HOME fallback)
  - schema bootstrap (meta, runtime, events)
  - pragma setup (WAL, busy_timeout, NORMAL)
  - runtime upsert API
  - event append API
  - retention trim API
- No integration in main.c yet (dead code for now is okay if linked)
Validation
- Build succeeds
- New unit tests for schema + insert + trim (if included in same commit)
---
2) watcher: wire runtime/event writes into main flow
Goal: make daemon publish state/history while remaining standalone.
- Update bandcamp_watcher/main.c
  - open/init DB at startup
  - set runtime starting → running
  - append watcher_started
  - heartbeat updates in watch loop
  - append album_copied / copy_failed / album_skipped
  - on oneshot completion append oneshot_completed
  - append watcher_stopped on exit/signal
  - close DB cleanly
- Optional minimal signal-safe shutdown hook (if you already have signal handling, piggyback there)
Validation
- xcodebuild test ... all pass
- manual:
  - run --oneshot
  - inspect DB rows via sqlite3 query (runtime + events)
---
3) tests: add state db and integration coverage
Goal: lock behavior before UI work.
- Add tests/StateDbTests.m
  - schema creation
  - runtime row upsert
  - event append
  - retention trim
- Update existing tests only where necessary for new dependencies
- Keep tests deterministic (temporary DB path)
Validation
- full test suite passes
- no flaky timing assertions
---
4) status-app: add Objective-C menubar target skeleton
Goal: introduce separate UI target (observer-only), no control plane.
> You said you’ll create target manually; this commit assumes target exists first.
- New files (target membership: status app only):
  - status_app/BCWServiceMonitor.h/.m
  - status_app/BCWStateStore.h/.m
  - status_app/BCWStatusController.h/.m
  - status_app/AppDelegate.m (or equivalent entrypoint)
- Implement:
  - menu bar icon + menu scaffold
  - periodic refresh (5s)
  - launchd/SMAppService status check abstraction
  - read runtime + recent copied albums from sqlite
  - actions: refresh/open config/open DB folder/quit
Validation
- app launches as menu bar extra (LSUIElement)
- shows stopped/running states correctly
- shows recent albums when daemon has data
---
5) status-app: polish status rendering and empty/error states
Goal: make UI production-usable and resilient.
- Add stale heartbeat logic
- Better status text mapping:
  - running / enabled-not-reporting / disabled / unknown
- Empty-state rows:
  - “No state yet”
  - “No recent copies”
- Handle DB unavailable gracefully (no crash)
Validation
- manual scenarios:
  1. daemon not running
  2. daemon running
  3. daemon writes events while UI open
  4. DB missing/corrupt
---
Suggested PR Boundaries
- PR 1: Commits 1–3 (backend + tests), no UI
- PR 2: Commits 4–5 (status app target + UI behavior)
This keeps risk low and review focused.
Notes on Review/Style
- Keep daemon behavior unchanged if DB fails (warn only)
- Keep stderr logging untouched (--oneshot remains useful)
- Avoid introducing write-path from UI to daemon in this phase
- Don’t rely on logs as source of truth; DB is canonical
