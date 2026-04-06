//
//  qobuz.h
//  bandcamp_watcher
//
//  Qobuz-specific file validation
//

#ifndef QOBUZ_H
#define QOBUZ_H

#include <stddef.h>
#include <limits.h>
#include <sys/types.h>
#include "bandcamp.h"

/* Check if files in directory match Qobuz naming pattern
 * Validates:
 * - Files have sequential numbering (01, 02, 03...)
 * - No gaps in sequence (must be exactly 1 to N)
 * - Track count is 3-30
 * - All files are same type (no mixed audio formats)
 * - Non-audio files (non-music) <= 6
 * Returns 0 if valid, -1 otherwise
 */
int check_qobuz_files(const char *path, band_info_t *band_info, 
                      const char **exts, int num_exts);

#endif /* QOBUZ_H */
