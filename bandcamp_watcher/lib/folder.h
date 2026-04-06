//
//  folder.h
//  bandcamp_watcher
//
//  Shared folder name parsing utilities for music source detection
//

#ifndef FOLDER_H
#define FOLDER_H

#include <stddef.h>

// Parse folder name in format "Artist - Album" with optional -N suffix
// The suffix (e.g., "-2") is stripped from the album name
// Returns 0 on success, -1 if format is invalid
int parse_folder_name(const char *folder, char *name, size_t name_len, 
                      char *album, size_t album_len);

#endif /* FOLDER_H */
