//
//  bandcamp.h
//  bandcamp_watcher
//
//  Bandcamp-specific file validation
//

#ifndef BANDCAMP_H
#define BANDCAMP_H

#include <stddef.h>
#include <limits.h>
#include <sys/types.h>
#include "folder.h"

/* Count files matching Bandcamp song naming pattern
 * Files should be named: "Band - Album - NN Song Name.ext"
 * Returns count of matching files
 */
size_t files_that_look_like_songs(const char *path, const char *band_name, 
                                   const char *album_name, const char *file_type);

/* Check if files in directory match Bandcamp naming pattern
 * Validates:
 * - Files contain band_name and album_name prefix
 * - All files are same type (no mixed audio formats)
 * - File count > 0
 * 
 * Parameters:
 *   path - Directory path to check
 *   band_info - Contains name and album (input), file_type set on success (output)
 *   exts - Array of file extensions to check (from config)
 *   num_exts - Number of extensions in array
 * 
 * Returns 0 if valid, -1 otherwise
 */
int check_bandcamp_files(const char *path, band_info_t *band_info, 
                         const char **exts, int num_exts);

#endif /* BANDCAMP_H */
