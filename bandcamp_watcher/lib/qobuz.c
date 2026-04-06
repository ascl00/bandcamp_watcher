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
    int max_track = 0;
    
    // Check for sequential numbering
    size_t sequential_files = files_with_sequential_numbers(path, exts, num_exts, &max_track);
    
    if (sequential_files == 0) {
        log_debug("No sequential track files found in %s", path);
        return -1;
    }
    
    // Validate track count (3-30)
    if (sequential_files < 3 || sequential_files > 30) {
        log_debug("Track count %zu is outside valid range (3-30) in %s", sequential_files, path);
        return -1;
    }
    
    // Get total file count
    size_t total_files = total_files_in_dir(path);
    
    // Check non-audio file count (must be <= 6)
    size_t non_audio = total_files - sequential_files;
    if (non_audio > 6) {
        log_debug("Too many non-audio files (%zu) in %s", non_audio, path);
        return -1;
    }
    
    // Determine file type from the files we found
    // Check first file to get extension
    DIR *d = opendir(path);
    if (!d) {
        log_error("Failed to open dir %s", path);
        return -1;
    }
    
    int found_type = 0;
    struct dirent *de = NULL;
    
    while ((de = readdir(d)) != NULL && !found_type) {
        if (de->d_type != DT_REG) continue;
        
        // Check if this file has a track number prefix
        char *endptr;
        int track_num = (int)strtol(de->d_name, &endptr, 10);
        
        if (track_num > 0 && endptr != de->d_name && !isdigit((unsigned char)*endptr)) {
            // This is a track file, check which extension matches
            for (int i = 0; i < num_exts; i++) {
                char ext_pattern[32];
                snprintf(ext_pattern, sizeof(ext_pattern), ".%s", exts[i]);
                if (is_matching_extension(de->d_name, ext_pattern) == 0) {
                    strcpy(band_info->file_type, exts[i]);
                    found_type = 1;
                    break;
                }
            }
        }
    }
    
    (void)closedir(d);
    
    if (!found_type) {
        log_error("Could not determine file type in %s", path);
        return -1;
    }
    
    // Check for mixed audio types
    for (int i = 0; i < num_exts; i++) {
        // Skip the type we already identified
        if (strcmp(exts[i], band_info->file_type) == 0) continue;
        
        char ext_pattern[32];
        snprintf(ext_pattern, sizeof(ext_pattern), ".%s", exts[i]);
        
        DIR *check_dir = opendir(path);
        if (!check_dir) continue;
        
        struct dirent *check_de = NULL;
        while ((check_de = readdir(check_dir)) != NULL) {
            if (check_de->d_type != DT_REG) continue;
            
            // Check if this file has a track number prefix
            char *endptr;
            int track_num = (int)strtol(check_de->d_name, &endptr, 10);
            
            if (track_num > 0 && endptr != check_de->d_name && !isdigit((unsigned char)*endptr)) {
                if (is_matching_extension(check_de->d_name, ext_pattern) == 0) {
                    log_error("Mixed audio types found in %s (found %s and %s)", 
                              path, band_info->file_type, exts[i]);
                    (void)closedir(check_dir);
                    return -1;
                }
            }
        }
        (void)closedir(check_dir);
    }
    
    log_trace("Found %zu Qobuz-style track files (%s) in %s", sequential_files, band_info->file_type, path);
    
    return 0;
}
