//
//  state_db.h
//  bandcamp_watcher
//
//  SQLite state database for runtime status and event history
//

#ifndef STATE_DB_H
#define STATE_DB_H

#include <sqlite3.h>
#include <stddef.h>
#include <stdint.h>

// Event type constants
typedef enum {
    EVENT_WATCHER_STARTED = 1,
    EVENT_WATCHER_STOPPED,
    EVENT_WATCHER_ERROR,
    EVENT_ALBUM_COPIED,
    EVENT_ALBUM_SKIPPED,
    EVENT_COPY_FAILED,
    EVENT_ONESHOT_COMPLETED
} event_type_t;

// Runtime status constants
typedef enum {
    RUNTIME_STATUS_STARTING = 1,
    RUNTIME_STATUS_RUNNING,
    RUNTIME_STATUS_IDLE,
    RUNTIME_STATUS_STOPPING,
    RUNTIME_STATUS_STOPPED,
    RUNTIME_STATUS_ERROR
} runtime_status_t;

// Runtime mode constants
typedef enum {
    RUNTIME_MODE_WATCH = 1,
    RUNTIME_MODE_ONESHOT
} runtime_mode_t;

// Source type (matches bandcamp.h SOURCE_* defines)
typedef enum {
    SOURCE_TYPE_BANDCAMP = 1,
    SOURCE_TYPE_QOBUZ = 2
} album_source_type_t;

// Opaque handle
struct state_db_s {
    sqlite3 *db;
    sqlite3_stmt *stmt_heartbeat;
    sqlite3_stmt *stmt_event;
    char *db_path;
};
typedef struct state_db_s state_db_t;

// Open/close
int state_db_open(const char *override_path, state_db_t **db_out);
void state_db_close(state_db_t *db);

// Runtime management
int state_db_init_runtime(state_db_t *db, runtime_mode_t mode, int pid);
int state_db_set_runtime_status(state_db_t *db, runtime_status_t status, const char *error_msg);
int state_db_heartbeat(state_db_t *db);
int state_db_set_last_scan(state_db_t *db);
int state_db_get_runtime(state_db_t *db, int *pid, runtime_mode_t *mode, 
                          runtime_status_t *status, int64_t *started_at, 
                          int64_t *heartbeat_at, int64_t *last_scan_at);

// Event management
int state_db_append_event(state_db_t *db, event_type_t type,
                          const char *artist, const char *album,
                          const char *file_type, album_source_type_t source_type,
                          const char *src_path, const char *dest_path,
                          const char *message, int exit_code);

// Maintenance
int state_db_trim(state_db_t *db, int max_events);

// Utility
const char *state_db_get_path(state_db_t *db);

#endif /* STATE_DB_H */
