//
//  state_db.c
//  bandcamp_watcher
//
//  SQLite state database for runtime status and event history
//

#include "state_db.h"
#include "log.h"

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <errno.h>

#define SCHEMA_VERSION 1
#define MAX_EVENTS_DEFAULT 5000
#define DB_SUBDIR "bandcamp_watcher"
#define DB_FILENAME "state.sqlite3"

static const char *create_schema_sql =
    "PRAGMA user_version = 1;"
    "PRAGMA journal_mode = WAL;"
    "PRAGMA synchronous = NORMAL;"
    "PRAGMA busy_timeout = 2000;"
    ""
    "CREATE TABLE IF NOT EXISTS meta ("
    "    key TEXT PRIMARY KEY NOT NULL,"
    "    value TEXT NOT NULL"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS runtime ("
    "    id INTEGER PRIMARY KEY CHECK (id = 1),"
    "    pid INTEGER,"
    "    mode INTEGER NOT NULL,"
    "    status INTEGER NOT NULL,"
    "    started_at INTEGER,"
    "    heartbeat_at INTEGER,"
    "    last_error TEXT,"
    "    last_scan_at INTEGER"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS events ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    ts INTEGER NOT NULL,"
    "    type INTEGER NOT NULL,"
    "    artist TEXT,"
    "    album TEXT,"
    "    file_type TEXT,"
    "    source_type INTEGER,"
    "    src_path TEXT,"
    "    dest_path TEXT,"
    "    message TEXT,"
    "    exit_code INTEGER"
    ");"
    ""
    "CREATE INDEX IF NOT EXISTS idx_events_ts ON events(ts DESC);"
    "CREATE INDEX IF NOT EXISTS idx_events_type_ts ON events(type, ts DESC);"
    ""
    "INSERT OR REPLACE INTO meta (key, value) VALUES ('schema_version', '1');";

static char *get_state_db_path(const char *override_path)
{
    char *path = NULL;
    
    if (override_path) {
        path = strdup(override_path);
        return path;
    }
    
    // Try XDG_STATE_HOME
    const char *xdg_state = getenv("XDG_STATE_HOME");
    if (xdg_state && *xdg_state) {
        path = malloc(PATH_MAX);
        if (path) {
            snprintf(path, PATH_MAX, "%s/%s/%s", xdg_state, DB_SUBDIR, DB_FILENAME);
        }
        return path;
    }
    
    // Fallback to ~/.local/state
    const char *home = getenv("HOME");
    if (home && *home) {
        path = malloc(PATH_MAX);
        if (path) {
            snprintf(path, PATH_MAX, "%s/.local/state/%s/%s", home, DB_SUBDIR, DB_FILENAME);
        }
        return path;
    }
    
    return NULL;
}

static int ensure_directory_exists(const char *path)
{
    char tmp[PATH_MAX];
    strncpy(tmp, path, PATH_MAX - 1);
    tmp[PATH_MAX - 1] = '\0';
    
    // Find parent directory
    char *last_slash = strrchr(tmp, '/');
    if (!last_slash || last_slash == tmp) {
        return 0;  // No parent or root
    }
    
    *last_slash = '\0';
    
    struct stat st;
    if (stat(tmp, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;  // Already exists
        }
        return -1;  // Exists but not a directory
    }
    
    // Create parent directories recursively
    if (ensure_directory_exists(tmp) != 0) {
        return -1;
    }
    
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    
    return 0;
}

int state_db_open(const char *override_path, state_db_t **db_out)
{
    if (!db_out) return -1;
    
    state_db_t *db = calloc(1, sizeof(state_db_t));
    if (!db) return -1;
    
    db->db_path = get_state_db_path(override_path);
    if (!db->db_path) {
        free(db);
        return -1;
    }
    
    // Ensure directory exists
    if (ensure_directory_exists(db->db_path) != 0) {
        log_warn("Failed to create state db directory for %s", db->db_path);
        free(db->db_path);
        free(db);
        return -1;
    }
    
    // Open database
    int rc = sqlite3_open(db->db_path, &db->db);
    if (rc != SQLITE_OK) {
        log_error("Failed to open state db: %s", sqlite3_errmsg(db->db));
        free(db->db_path);
        free(db);
        return -1;
    }
    
    // Create schema
    rc = sqlite3_exec(db->db, create_schema_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        log_error("Failed to create schema: %s", sqlite3_errmsg(db->db));
        sqlite3_close(db->db);
        free(db->db_path);
        free(db);
        return -1;
    }
    
    // Prepare statements
    const char *heartbeat_sql = "UPDATE runtime SET heartbeat_at = ? WHERE id = 1;";
    rc = sqlite3_prepare_v2(db->db, heartbeat_sql, -1, &db->stmt_heartbeat, NULL);
    if (rc != SQLITE_OK) {
        log_error("Failed to prepare heartbeat statement: %s", sqlite3_errmsg(db->db));
        // Non-fatal, will try again later
    }
    
    const char *event_sql = 
        "INSERT INTO events (ts, type, artist, album, file_type, source_type, "
        "src_path, dest_path, message, exit_code) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db->db, event_sql, -1, &db->stmt_event, NULL);
    if (rc != SQLITE_OK) {
        log_error("Failed to prepare event statement: %s", sqlite3_errmsg(db->db));
        // Non-fatal, will try again later
    }
    
    log_debug("Opened state db at %s", db->db_path);
    *db_out = db;
    return 0;
}

void state_db_close(state_db_t *db)
{
    if (!db) return;
    
    if (db->stmt_heartbeat) {
        sqlite3_finalize(db->stmt_heartbeat);
    }
    if (db->stmt_event) {
        sqlite3_finalize(db->stmt_event);
    }
    if (db->db) {
        sqlite3_close(db->db);
    }
    if (db->db_path) {
        free(db->db_path);
    }
    free(db);
}

int state_db_init_runtime(state_db_t *db, runtime_mode_t mode, int pid)
{
    if (!db || !db->db) return -1;
    
    int64_t now = time(NULL);
    
    const char *sql = 
        "INSERT OR REPLACE INTO runtime "
        "(id, pid, mode, status, started_at, heartbeat_at, last_error, last_scan_at) "
        "VALUES (1, ?, ?, ?, ?, ?, NULL, NULL);";
    
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_warn("Failed to prepare init_runtime: %s", sqlite3_errmsg(db->db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, pid);
    sqlite3_bind_int(stmt, 2, mode);
    sqlite3_bind_int(stmt, 3, RUNTIME_STATUS_STARTING);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_bind_int64(stmt, 5, now);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        log_warn("Failed to init runtime: %s", sqlite3_errmsg(db->db));
        return -1;
    }
    
    return 0;
}

int state_db_set_runtime_status(state_db_t *db, runtime_status_t status, const char *error_msg)
{
    if (!db || !db->db) return -1;
    
    const char *sql = 
        "UPDATE runtime SET status = ?, last_error = ? WHERE id = 1;";
    
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_warn("Failed to prepare set_status: %s", sqlite3_errmsg(db->db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, status);
    sqlite3_bind_text(stmt, 2, error_msg ? error_msg : NULL, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        log_warn("Failed to set runtime status: %s", sqlite3_errmsg(db->db));
        return -1;
    }
    
    return 0;
}

int state_db_heartbeat(state_db_t *db)
{
    if (!db || !db->db) return -1;
    
    // Use cached statement if available
    sqlite3_stmt *stmt = db->stmt_heartbeat;
    if (!stmt) {
        const char *sql = "UPDATE runtime SET heartbeat_at = ? WHERE id = 1;";
        int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            log_warn("Failed to prepare heartbeat: %s", sqlite3_errmsg(db->db));
            return -1;
        }
        db->stmt_heartbeat = stmt;
    }
    
    int64_t now = time(NULL);
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, now);
    
    int rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_DONE) {
        log_warn("Failed to update heartbeat: %s", sqlite3_errmsg(db->db));
        return -1;
    }
    
    return 0;
}

int state_db_set_last_scan(state_db_t *db)
{
    if (!db || !db->db) return -1;
    
    const char *sql = "UPDATE runtime SET last_scan_at = ? WHERE id = 1;";
    
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_warn("Failed to prepare set_last_scan: %s", sqlite3_errmsg(db->db));
        return -1;
    }
    
    int64_t now = time(NULL);
    sqlite3_bind_int64(stmt, 1, now);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        log_warn("Failed to set last scan: %s", sqlite3_errmsg(db->db));
        return -1;
    }
    
    return 0;
}

int state_db_get_runtime(state_db_t *db, int *pid, runtime_mode_t *mode, 
                          runtime_status_t *status, int64_t *started_at, 
                          int64_t *heartbeat_at, int64_t *last_scan_at)
{
    if (!db || !db->db) return -1;
    
    const char *sql = 
        "SELECT pid, mode, status, started_at, heartbeat_at, last_scan_at "
        "FROM runtime WHERE id = 1;";
    
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        if (pid) *pid = sqlite3_column_int(stmt, 0);
        if (mode) *mode = sqlite3_column_int(stmt, 1);
        if (status) *status = sqlite3_column_int(stmt, 2);
        if (started_at) *started_at = sqlite3_column_int64(stmt, 3);
        if (heartbeat_at) *heartbeat_at = sqlite3_column_int64(stmt, 4);
        if (last_scan_at) *last_scan_at = sqlite3_column_int64(stmt, 5);
        sqlite3_finalize(stmt);
        return 0;
    }
    
    sqlite3_finalize(stmt);
    return -1;
}

int state_db_append_event(state_db_t *db, event_type_t type,
                          const char *artist, const char *album,
                          const char *file_type, album_source_type_t source_type,
                          const char *src_path, const char *dest_path,
                          const char *message, int exit_code)
{
    if (!db || !db->db) return -1;
    
    // Use cached statement if available
    sqlite3_stmt *stmt = db->stmt_event;
    if (!stmt) {
        const char *sql = 
            "INSERT INTO events (ts, type, artist, album, file_type, source_type, "
            "src_path, dest_path, message, exit_code) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            log_warn("Failed to prepare event: %s", sqlite3_errmsg(db->db));
            return -1;
        }
        db->stmt_event = stmt;
    }
    
    int64_t now = time(NULL);
    
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_int(stmt, 2, type);
    sqlite3_bind_text(stmt, 3, artist ? artist : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, album ? album : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, file_type ? file_type : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, source_type);
    sqlite3_bind_text(stmt, 7, src_path ? src_path : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, dest_path ? dest_path : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, message ? message : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 10, exit_code);
    
    log_debug("Appending event: type=%d, artist='%s', album='%s'", type, 
              artist ? artist : "(null)", album ? album : "(null)");
    
    int rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_DONE) {
        log_warn("Failed to append event: %s", sqlite3_errmsg(db->db));
        return -1;
    }
    
    return 0;
}

int state_db_trim(state_db_t *db, int max_events)
{
    if (!db || !db->db) return -1;
    if (max_events <= 0) max_events = MAX_EVENTS_DEFAULT;
    
    const char *sql = 
        "DELETE FROM events WHERE id <= ("
        "  SELECT id FROM events ORDER BY ts DESC LIMIT 1 OFFSET ?"
        ");";
    
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_warn("Failed to prepare trim: %s", sqlite3_errmsg(db->db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, max_events);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        log_warn("Failed to trim events: %s", sqlite3_errmsg(db->db));
        return -1;
    }
    
    int deleted = sqlite3_changes(db->db);
    if (deleted > 0) {
        log_debug("Trimmed %d old events from state db", deleted);
    }
    
    return 0;
}

const char *state_db_get_path(state_db_t *db)
{
    if (!db) return NULL;
    return db->db_path;
}
