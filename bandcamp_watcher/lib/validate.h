//
//  validate.h
//  bandcamp_watcher
//
//  Unified music folder validation for Bandcamp and Qobuz sources
//

#ifndef VALIDATE_H
#define VALIDATE_H

#include "bandcamp.h"

// Source types detected
#define SOURCE_UNKNOWN  0
#define SOURCE_BANDCAMP 1
#define SOURCE_QOBUZ    2

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

#endif /* VALIDATE_H */
