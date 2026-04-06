//
//  bandcamp.c
//  bandcamp_watcher
//
//  Bandcamp-specific file validation
//

#include "bandcamp.h"
#include "utils.h"
#include "log.h"

#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>

/* Count files matching Bandcamp song naming pattern
 * Files should be named: "Band - Album - NN Song Name.ext"
 * Returns count of matching files
 */
size_t files_that_look_like_songs(const char *path, const char *band_name, 
                                   const char *album_name, const char *file_type)
{
    DIR *d = opendir(path);
    size_t count = 0;
    char ext_pattern[32];
    
    if(!d)
    {
        log_error("Failed to open dir %s", path);
        return 0;
    }
    
    // Build extension pattern (e.g., ".flac" or ".m4a")
    snprintf(ext_pattern, sizeof(ext_pattern), ".%s", file_type);
    
    struct dirent *de = NULL;
    
    while ((de = readdir(d)) != NULL)
    {
        if (de->d_type == DT_REG && is_matching_extension(de->d_name, ext_pattern) == 0)
        {
            // <bandname> - <album name> - <track number> <song name>.<file format>
            char song_name[NAME_MAX+1];
            int chars = sprintf(song_name, "%s - %s - ", band_name, album_name);
            
            // Skip if filename is too short to possibly match
            size_t name_len = strlen(de->d_name);
            if(name_len < (size_t)(chars + 5))  // +5 for minimum "01 x.ext"
            {
                log_trace("filename isn't long enough (%s/%s) to match expected formatting", path, de->d_name);
                continue;
            }
            
            if(name_compare(song_name, de->d_name, strlen(song_name)) == 0)
            {
                // Looks good!
                count++;
            }
        }
    }
    
    (void)closedir(d);
    
    return count;
}

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
                         const char **exts, int num_exts)
{
    DIR *d = opendir(path);
    if(!d)
    {
        log_error("Failed to open dir %s", path);
        return -1;
    }
    
    // Check for each audio type
    size_t type_counts[num_exts];
    memset(type_counts, 0, sizeof(type_counts));
    int found_type = -1;
    
    // First pass: count files of each type
    struct dirent *de = NULL;
    while ((de = readdir(d)) != NULL)
    {
        if (de->d_type != DT_REG) continue;
        
        for (int i = 0; i < num_exts; i++) {
            char ext_pattern[32];
            snprintf(ext_pattern, sizeof(ext_pattern), ".%s", exts[i]);
            if (is_matching_extension(de->d_name, ext_pattern) == 0) {
                type_counts[i]++;
                break;
            }
        }
    }
    
    rewinddir(d);
    
    // Check for mixed types
    int types_found = 0;
    for (int i = 0; i < num_exts; i++) {
        if (type_counts[i] > 0) {
            types_found++;
            found_type = i;
        }
    }
    
    if (types_found == 0) {
        log_debug("No audio files found in %s", path);
        (void)closedir(d);
        return -1;
    }
    
    if (types_found > 1) {
        log_error("Found mixed audio types in %s", path);
        (void)closedir(d);
        return -1;
    }
    
    // We have exactly one audio type
    strcpy(band_info->file_type, exts[found_type]);
    
    // Check song naming pattern
    size_t song_files = files_that_look_like_songs(path, band_info->name, 
                                                    band_info->album, 
                                                    band_info->file_type);
    
    (void)closedir(d);
    
    if (song_files == 0) {
        log_info("Found audio files but not in Bandcamp naming format. Skipping (%s)", path);
        return -1;
    }
    
    log_trace("Found %zu Bandcamp-style song files (%s)", song_files, band_info->file_type);
    
    return 0;
}

const char *band_info_string(band_info_t *bi)
{
    static char ret[NAME_MAX+1];
    ret[0]='\0';
    strcat(ret, bi->name);
    strcat(ret, " -> ");
    strcat(ret, bi->album);
    strcat(ret, " (");
    strcat(ret, bi->file_type);
    strcat(ret, ")");
    return ret;
}
