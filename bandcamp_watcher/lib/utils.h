//
//  utils.h
//  bandcamp_watcher
//
//  String and file utilities
//

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/* Similar to strncasecmp however wont fail a match if puncuation characters mismatch */
int name_compare(const char* s1, const char* s2, size_t len);

/* Check if filename ends with given extension (case-insensitive) */
/* Returns 0 if matches, non-zero otherwise */
int is_matching_extension(const char *filename, const char *ext);

/* Trim whitespace from string (modifies in place, returns pointer to trimmed string) */
char *trim(char *str);

/* Parse boolean string (case-insensitive)
 * Returns: 1 for "true", "yes", "1", "on"
 *          0 for "false", "no", "0", "off", or unrecognized
 */
int parse_bool(const char *str);

#endif /* UTILS_H */
