//
//  bandcamp.h
//  bandcamp_watcher
//
//  Bandcamp folder detection and validation
//

#ifndef BANDCAMP_H
#define BANDCAMP_H

#include <stddef.h>
#include <limits.h>

typedef enum {
    flac,
    aac,
    unknown
} file_type_e;

typedef struct {
    char name[NAME_MAX+1];
    char album[NAME_MAX+1];
    file_type_e file_type;
} band_info_t;

/* Count files with given extension in directory */
size_t files_with_extension(const char *path, const char *ext);

/* Count files matching Bandcamp song naming pattern */
size_t files_that_look_like_songs(const char *path, const char *band_name, const char *album_name, file_type_e file_type);

/* Validate if folder looks like a Bandcamp download */
int check_for_bandcamp_folder(const char *fullpath, const char *folder, band_info_t *band_info);

/* Format band_info_t as readable string */
const char *band_info_string(band_info_t *bi);

#endif /* BANDCAMP_H */
