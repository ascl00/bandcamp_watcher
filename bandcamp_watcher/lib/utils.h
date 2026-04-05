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

#endif /* UTILS_H */
