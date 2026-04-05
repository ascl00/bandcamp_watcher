//
//  bandcamp.c
//  bandcamp_watcher
//
//  Bandcamp folder detection and validation
//

#include "bandcamp.h"
#include "utils.h"
#include "log.h"

#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

size_t files_with_extension(const char *path, const char *ext)
{
    DIR *d = opendir(path);
    size_t count = 0;
    
    if(!d)
    {
        log_error("Failed to open dir %s - %d:%s", path, errno, strerror(errno));
        return count;
    }
    struct dirent *de = NULL;
    
    while ( (de = readdir(d)) != NULL)
    {
        if (de->d_type == DT_REG && is_matching_extension(de->d_name, ext) == 0)
        {
            count++;
        }
    }
    
    (void)closedir(d);
    
    return count;
}

size_t files_that_look_like_songs(const char *path, const char *band_name, const char *album_name, file_type_e file_type)
{
    DIR *d = opendir(path);
    size_t count = 0;
    
    if(!d)
    {
        log_error("Failed to open dir %s - %d:%s", path, errno, strerror(errno));
        return count;
    }
    struct dirent *de = NULL;
    
    while ( (de = readdir(d)) != NULL)
    {
        if (de->d_type == DT_REG && is_matching_extension(de->d_name, file_type==flac?".flac":".m4a") == 0)
        {
            // <bandname> - <album name> - <track number> <song name>.<file format>
            char song_name[NAME_MAX+1];
            int chars = sprintf(song_name, "%s - %s - ", band_name, album_name);
            if(de->d_namlen < chars+5)
            {
                log_trace("filename isn't long enough (%s/%s) to match expected formatting",path, de->d_name);
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

const char *band_info_string(band_info_t *bi)
{
    static char ret[NAME_MAX+1];
    ret[0]='\0';
    strcat(ret, bi->name);
    strcat(ret, " -> ");
    strcat(ret, bi->album);
    strcat(ret, bi->file_type==flac?" (flac)":bi->file_type==aac?" (aac)":" (unknown)");
    return ret;
}

int check_for_bandcamp_folder(const char *fullpath, const char *folder, band_info_t *band_info)
{
    // We want to see if this looks like a folder downloaded from Bandcamp (and automatically decompressed by Safari/Finder)
    // Typical structure is <bandname> - <album name> possibly with a "-<number>" appended if there are duplicate folder names
    // Contents should have format <bandname> - <album name> - <track number> <song name>.<file format>
    // From this we should extract
    //              - If it is a bandcamp looking download
    //              - the bandname and the album name
    
    // First lets just check if we have at least one dash
    const char *p = folder;
    while(*p)
    {
        if(*p == '-')
            break;
        p++;
    }
    
    if(*p != '-')
    {
        log_debug("Failed to find a dash in %s", folder);
        return -1;
    }
    // lets check that this folder name doesn't start with a dash (making p-1 invalid)
    if(p==folder)
    {
        log_debug("Dash at start of %s", folder);
        return -1;
    }
    
    // lets capture the end of the first part of the string for later use
    const char *band_name_end = p-1;
    
    p++;
    if(!*p)
    {
        // Hit end of string already, not what we are looking for
        log_debug("Only dash is trailing in %s", folder);
        return -1;
    }
    // A dash is appended when Safari downloads the same file multiple times,  so lets just check that isn't the case here.
    while(*p && *p >= '0' && *p <= '9')
    {
        p++;
    }
    
    if(!*p)
    {
        // the only dash is the one at the end, this isn't helping us
        log_debug("No dash in the middle in %s", folder);
        return -1;
    }
    
    if(*p != ' ')
    {
        log_debug("Expected space after dash in %s", folder);
        return -1;
    }
    const char *album_name_start = p+1; //
    if(*album_name_start == '\0')
    {
        log_debug("Unexpected end of name in %s", folder);
        return -1;
    }

    
    // Easy checks done, lets check the subfolder contents and see what we can see
    size_t flac_files = files_with_extension(fullpath, ".flac");
    if(flac_files == 0)
    {
        log_debug("failed to find any flac files in %s", folder);
    }
    
    size_t aac_files = files_with_extension(fullpath, ".m4a");
    if(aac_files == 0)
    {
        log_debug("failed to find any aac files in %s", folder);
    }
    
    if(flac_files == 0 && aac_files == 0)
    {
        log_trace("failed to find any aac or flac files in %s", folder);
        return -1;
    }
    
    if(flac_files && aac_files)
    {
        // Both flac and aac? I dunno whats going on here, lets bail out
        log_error("Found both AAC and FLAC files? that's... odd and I don't know what to do (%s)", folder);
        return -1;
    }
    
    // Okay at this point, we might have a bandcamp folder, we have the right structure and some audio files, but we can check a bit harder
    // since we know that the audio files inside the folder should have this format:
    // <bandname> - <album name> - <track number> <song name>.<file format>
    // So lets extract band and album names
        
    strncpy(band_info->name, folder, band_name_end-folder);
    band_info->name[band_name_end-folder]='\0';
    strcpy(band_info->album, album_name_start);
    
    // We may need to strip off some garbage at the end tho
    char *dash = strrchr(band_info->album, '-');
    if(dash)
    {
        p = dash+1;
        while(*p && *p >= '0' && *p <= '9')
        {
            p++;
        }
        
        if(*p == '\0')
        {
            // We got to the end of the string and only found numbers, lets assume it is a folder-2 arrangement
            log_trace("found trailing -<numbers> in %s, removing them", band_info->album);
            *dash = '\0';
        }
    }
    
    // We might have '(pre-order)' at the end of the album name, we should remove that too.
    char *parens = strrchr(band_info->album, '(');
    if(parens != NULL)
    {
        if(strcmp(parens, "(pre-order)") == 0)
        {
            parens--;
            *parens='\0';
        }
    }
    
    if(flac_files)
    {
        band_info->file_type=flac;
    }
    else if(aac_files)
    {
        band_info->file_type=aac;
    }
    
    // Check some song files
    size_t song_files = files_that_look_like_songs(fullpath, band_info->name, band_info->album, band_info->file_type);
    
    if(song_files == 0)
    {
        log_info("Found a folder, with audio files, but not in the format expected. Skipping (%s)", folder);
        return -1;
    }

    log_trace("Found %zu song_files, there was %zu audio files", song_files, aac_files+flac_files);
    
    log_trace("%s", band_info_string(band_info));

    return 0;
}
