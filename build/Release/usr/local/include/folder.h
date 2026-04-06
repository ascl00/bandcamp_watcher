//
//  folder.h
//  bandcamp_watcher
//
//  Shared folder name parsing and validation for music source detection
//

#ifndef FOLDER_H
#define FOLDER_H

#include <stddef.h>
#include <limits.h>
#include <sys/types.h>

typedef struct {
    char name[NAME_MAX+1];
    char album[NAME_MAX+1];
    char file_type[16];  // "flac", "aac", "m4a", etc.
} band_info_t;

// Source types detected
#define SOURCE_UNKNOWN  0
#define SOURCE_BANDCAMP 1
#define SOURCE_QOBUZ    2

// Parse folder name in format "Artist - Album" with optional -N suffix
// The suffix (e.g., "-2") is stripped from the album name
// Returns 0 on success, -1 if format is invalid
int parse_folder_name(const char *folder, char *name, size_t name_len,
                      char *album, size_t album_len);

/* Format band_info_t as readable string (Band -> Album (type)) */
const char *band_info_string(band_info_t *bi);

/* Unified folder check that validates music folder structure
 *
 * This function:
 * 1. Parses folder name using shared logic (Artist - Album format)
 * 2. First tries Bandcamp file validation
 * 3. If that fails, tries Qobuz file validation
 * 4. Applies common validation rules (non-audio count, etc.)
 *
 * Parameters:
 *   fullpath - Full path to the folder
 *   folder   - Folder name (just the last component)
 *   band_info - Output: Filled with band name, album, file type
 *   exts     - Array of file extensions to check (from config)
 *   num_exts - Number of extensions in array
 *   source_type - Output: Set to SOURCE_BANDCAMP or SOURCE_QOBUZ
 *
 * Returns 0 if valid music folder found, -1 otherwise
 */
int check_music_folder(const char *fullpath, const char *folder,
                       band_info_t *band_info, const char **exts, int num_exts,
                       int *source_type);

#endif /* FOLDER_H */
