//
//  qobuz.c
//  bandcamp_watcher
//
//  Qobuz-specific file validation
//

#include "qobuz.h"
#include "utils.h"
#include "log.h"

#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

int check_qobuz_files(const char *path, band_info_t *band_info,
                      const char **exts, int num_exts)
{
    DIR *d = opendir(path);
    size_t count = 0;
    size_t non_audio = 0;
    int highest = 0;
    int *found_tracks = NULL;
    int *type_counts = NULL;
    int types_found = 0;
    int first_type = -1;

    if(!d)
    {
        log_debug("Cannot open directory %s", path);
        return -1;
    }

    // Allocate array to track counts per type
    type_counts = calloc(num_exts, sizeof(int));
    if (!type_counts) {
        (void)closedir(d);
        log_error("Memory allocation failed for type_counts");
        return -1;
    }

    // First pass: find highest track number, count matching files per type, and count non-audio
    struct dirent *de = NULL;
    while ((de = readdir(d)) != NULL)
    {
        if (de->d_type != DT_REG) continue;

        // Check which extension matches (if any)
        int ext_matched = -1;
        for (int i = 0; i < num_exts; i++) {
            char ext_pattern[32];
            snprintf(ext_pattern, sizeof(ext_pattern), ".%s", exts[i]);
            if (is_matching_extension(de->d_name, ext_pattern) == 0)
            {
                ext_matched = i;
                break;
            }
        }

        if (ext_matched < 0) {
            // This is a non-audio file
            non_audio++;
            continue;
        }

        // Check for leading number
        char *endptr;
        int track_num = (int)strtol(de->d_name, &endptr, 10);

        // Valid track files should:
        // - Start with a number > 0
        // - Have a space or other non-digit after the number
        if (track_num > 0 && endptr != de->d_name && !isdigit((unsigned char)*endptr))
        {
            type_counts[ext_matched]++;
            if (type_counts[ext_matched] == 1) {
                types_found++;
                if (first_type < 0) first_type = ext_matched;
            }
            count++;
            if (track_num > highest) highest = track_num;
        }
    }

    rewinddir(d);

    // If no valid track files found in first pass, skip second pass
    if (first_type < 0 || highest == 0) {
        (void)closedir(d);
        free(type_counts);
        log_debug("No sequential track files found in %s", path);
        return -1;
    }

    // Allocate array to track which numbers we've seen
    found_tracks = calloc(highest + 1, sizeof(int));
    if (!found_tracks) {
        (void)closedir(d);
        free(type_counts);
        log_error("Memory allocation failed for found_tracks");
        return -1;
    }

    // Build extension pattern for the detected type only
    char ext_pattern[32];
    snprintf(ext_pattern, sizeof(ext_pattern), ".%s", exts[first_type]);

    // Second pass: mark which tracks we found (only check the detected file type)
    while ((de = readdir(d)) != NULL)
    {
        if (de->d_type != DT_REG) continue;

        // Only check the detected file type, not all extensions
        if (is_matching_extension(de->d_name, ext_pattern) != 0) continue;

        char *endptr;
        int track_num = (int)strtol(de->d_name, &endptr, 10);

        if (track_num > 0 && endptr != de->d_name && !isdigit((unsigned char)*endptr))
        {
            if (track_num <= highest) {
                found_tracks[track_num] = 1;
            }
        }
    }

    (void)closedir(d);

    // Check for gaps in sequence (1 to highest should all be present)
    int valid = 1;
    for (int i = 1; i <= highest; i++) {
        if (!found_tracks[i]) {
            valid = 0;
            break;
        }
    }

    free(found_tracks);
    free(type_counts);

    // Return -1 if sequence has gaps
    if (!valid) {
        log_debug("No sequential track files found in %s", path);
        return -1;
    }

    // Check for mixed audio types (detected in the same pass)
    if (types_found > 1) {
        log_error("Mixed audio types found in %s (%d different types)", path, types_found);
        return -1;
    }

    // Validate track count (3-30)
    if (count < 3 || count > 30) {
        log_debug("Track count %zu is outside valid range (3-30) in %s", count, path);
        return -1;
    }

    // Check non-audio file count (must be <= 6)
    if (non_audio > 6) {
        log_debug("Too many non-audio files (%zu) in %s", non_audio, path);
        return -1;
    }

    // Set the detected file type
    if (first_type >= 0 && first_type < num_exts) {
        strcpy(band_info->file_type, exts[first_type]);
    } else {
        log_error("Could not determine file type in %s", path);
        return -1;
    }

    log_trace("Found %zu Qobuz-style track files (%s) in %s", count, band_info->file_type, path);

    return 0;
}
