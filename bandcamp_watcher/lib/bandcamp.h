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

typedef struct {
    char name[NAME_MAX+1];
    char album[NAME_MAX+1];
    char file_type[16];  // "flac", "aac", "m4a", etc.
} band_info_t;

/* Count files matching Bandcamp song naming pattern
 * Files should be named: "Band - Album - NN Song Name.ext"
 * Returns count of matching files, 0 if mixed types found
 */
size_t files_that_look_like_songs(const char *path, const char *band_name, 
                                   const char *album_name, const char *file_type);

/* Check if files in directory match Bandcamp naming pattern
 * Validates:
 * - Files contain band_name and album_name prefix
 * - All files are same type (no mixed audio formats)
 * - File count > 0
 * Returns 0 if valid, -1 otherwise
 */
int check_bandcamp_files(const char *path, band_info_t *band_info);

/* Format band_info_t as readable string */
const char *band_info_string(band_info_t *bi);

#endif /* BANDCAMP_H */
