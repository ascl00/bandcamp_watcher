//
//  folder.c
//  bandcamp_watcher
//
//  Shared folder name parsing utilities for music source detection
//

#include "folder.h"
#include "log.h"
#include <string.h>
#include <ctype.h>

int parse_folder_name(const char *folder, char *name, size_t name_len, 
                      char *album, size_t album_len)
{
    if (!folder || !name || !album) return -1;
    
    // We want to see if this looks like a folder downloaded from a music service
    // Typical structure is <bandname> - <album name> possibly with a "-<number>" appended
    
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
    // A dash is appended when Safari downloads the same file multiple times, so lets just check that isn't the case here.
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

    // Extract band name
    size_t band_len = band_name_end - folder + 1;
    if (band_len >= name_len) band_len = name_len - 1;
    strncpy(name, folder, band_len);
    name[band_len] = '\0';
    
    // Trim trailing space from band name
    while (band_len > 0 && isspace((unsigned char)name[band_len-1])) {
        name[--band_len] = '\0';
    }
    
    // Extract album name
    strncpy(album, album_name_start, album_len - 1);
    album[album_len - 1] = '\0';
    
    // We may need to strip off some garbage at the end tho
    char *dash = strrchr(album, '-');
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
            log_trace("found trailing -<numbers> in %s, removing them", album);
            *dash = '\0';
        }
    }
    
    // We might have '(pre-order)' at the end of the album name, we should remove that too.
    char *parens = strrchr(album, '(');
    if(parens != NULL)
    {
        if(strcmp(parens, "(pre-order)") == 0)
        {
            parens--;
            *parens='\0';
        }
    }
    
    // Trim trailing whitespace from album
    size_t album_len_actual = strlen(album);
    while (album_len_actual > 0 && isspace((unsigned char)album[album_len_actual-1])) {
        album[--album_len_actual] = '\0';
    }
    
    return 0;
}
