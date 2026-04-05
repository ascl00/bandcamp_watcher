//
//  main.c
//  bandcamp_watcher
//
//  Created by Nick Blievers on 18/6/2025.
//

#include <errno.h>       // for errno
#include <fcntl.h>       // for O_RDONLY
#include <stdio.h>       // for fprintf()
#include <stdlib.h>      // for EXIT_SUCCESS
#include <ctype.h>
#include <string.h>      // for strerror()
#include <sys/event.h>   // for kqueue() etc.
#include <unistd.h>      // for close()
#include <dirent.h>      // for opendir()
#include <sys/stat.h>    // for stat()
#include <sys/time.h>    // for timespec_diff_macro()

#include "log/log.h"    // logging functions
#include "copy.h"

typedef struct {
    const char *folder; // folder to watch for new directories to be created in
    const char *flac_music_folder; // folder to copy FLAC files
    const char *aac_music_folder; // folder to copy AAC files
    struct timeval last_run;        // last time we processed an event (if ever)
} context_t;

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

/* Similar to strncasecmp however wont fail a match if puncuation characters mismatch */
int name_compare(const char* s1, const char* s2, size_t len)
{
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
    size_t n = 0;
    int result = 0;

    if (p1 == p2 || *p1 == '\0' || *p2 == '\0')
        return 0;

    do
    {
        char c1 = tolower(*p1);
        char c2 = tolower(*p2);
        
        if(isalnum(c1) && isalnum(c2))
        {
            result = c1 - c2;
            if(result != 0)
                break;
        }
    } while (*p1++ != '\0' && *p2++ != '\0' && n++ <= len);
    
    return result;
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

char *flagstring(int flags)
{
    static char ret[512];
    char *or = "";
 
    ret[0]='\0'; // clear the string.
    if (flags & NOTE_DELETE) {strcat(ret,or);strcat(ret,"NOTE_DELETE");or="|";}
    if (flags & NOTE_WRITE) {strcat(ret,or);strcat(ret,"NOTE_WRITE");or="|";}
    if (flags & NOTE_EXTEND) {strcat(ret,or);strcat(ret,"NOTE_EXTEND");or="|";}
    if (flags & NOTE_ATTRIB) {strcat(ret,or);strcat(ret,"NOTE_ATTRIB");or="|";}
    if (flags & NOTE_LINK) {strcat(ret,or);strcat(ret,"NOTE_LINK");or="|";}
    if (flags & NOTE_RENAME) {strcat(ret,or);strcat(ret,"NOTE_RENAME");or="|";}
    if (flags & NOTE_REVOKE) {strcat(ret,or);strcat(ret,"NOTE_REVOKE");or="|";}
 
    return ret;
}

int is_matching_extension(const char *filename, const char *ext)
{
    const char *p = filename + strlen(filename);
    
    p-=strlen(ext); // wind p back to where ext should start, ie ".ext"
    if(p<filename) // wound back before start of string
        return 0;
    
    return strcasecmp(p, ext);
}

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

int add_folder_to_apple_music(const char *folder)
{
    // kinda hacky but easier to call out to osascript than try and do it within C APIs.
    char cmd[NAME_MAX+1];
    // osascript -e "tell application \"Music\" to add Posix file \"/Volumes/Multimedia/Music/aac/Evile/The Unknown\""
    sprintf(cmd, "/usr/bin/osascript -e \"tell application \\\"Music\\\" to add Posix file \\\"%s\\\"\"", folder);
    return system(cmd);
}


int process(context_t *context)
{

    /* kqueue provides no information about the new files in the directory, simply that the directory vnode has changed
     * this makes things interesting. Do we try and maintain a list of paths we have checked previously? or do we just
     * scan all folders and hope its fast enough?
     */
    struct timeval start_of_run;
    gettimeofday(&start_of_run, NULL);
    
    DIR *dirp = opendir(context->folder);
    struct dirent *dp = NULL;
    
    if (dirp == NULL)
            return (-1);
 
    
    while ((dp = readdir(dirp)) != NULL)
    {
        if(dp->d_type == DT_DIR)
        {
            if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            {
                // skip
                continue;
            }
            char path[NAME_MAX+1];
            snprintf(path, NAME_MAX+1, "%s/%.*s", context->folder,  dp->d_namlen, dp->d_name);
            struct stat s = {};
            int res = stat(path, &s);
            if(res != 0)
            {
                log_error("Failed to stat %s, (%d) %s", path, errno, strerror(errno));
            }
            else
            {
                /*
                 * Compare s.st_birthtimespec against our previous run to avoid re-processing
                 * NOTE: Technically this introduces a race condition which may result in folders added after we record
                 * the start time, but before we have processed them. This would result in the folder being processed twice
                 * however I'm not seeing this as a big issue. We will check if the destination exists before processing anyway.
                 */
                struct timeval birthtimeval;
                TIMESPEC_TO_TIMEVAL(&birthtimeval, &s.st_birthtimespec);
                if(timercmp(&birthtimeval, &context->last_run, >= ))
                {
                    log_debug("Found new directory: %s -> %s", path, dp->d_name);
                    band_info_t band_info;
                    if(check_for_bandcamp_folder(path, dp->d_name, &band_info) == 0)
                    {
                        log_info("Found a folder that appears to be from bandcamp (%s)!", dp->d_name);
                        // Now we should check if our destination folder already contains this or not
                        char dst_path[NAME_MAX+1];
                        sprintf(dst_path, "%s/%s/%s", band_info.file_type==flac ? context->flac_music_folder
                                                                                :context->aac_music_folder,
                                                                                band_info.name, band_info.album);
                        char band_dst_path[NAME_MAX+1];
                        sprintf(band_dst_path, "%s/%s", band_info.file_type==flac ? context->flac_music_folder
                                                                                :context->aac_music_folder,
                                                                                band_info.name);
                        if(dir_exists(dst_path))
                        {
                            log_info("%s already exists, skipping...", dst_path);
                            continue;
                        }
                        
                        if(!dir_exists(band_dst_path))
                        {
                            // Band folder doesn't exist, lets create it
                            if(mkdir(band_dst_path, 0755))
                            {
                                perror(band_dst_path);
                                continue;
                            }
                        }
                        
                        // Okay lets copy stuff in to its final location
                        log_info("Copying files to %s", dst_path);
                        clone(path, dst_path);
                        
                        if(band_info.file_type == aac)
                        {
                            log_info("Adding %s to Apple music", dst_path);
                            add_folder_to_apple_music(dst_path);
                        }
                    }
                }
            }
        }
    }
    (void)closedir(dirp);
    context->last_run.tv_sec = start_of_run.tv_sec;
    context->last_run.tv_usec = start_of_run.tv_usec;

    return 0;
}

int watch_folder(const context_t *context)
{
    int kq;
    void *user_data = (void*)context;

    /* Open a kernel queue. */
    if ((kq = kqueue()) < 0) {
        log_error("Could not open kernel queue.  Error was %s.", strerror(errno));
        return -1;
    }

    int dirfd = open (context->folder, O_EVTONLY); // O_RDONLY ?
    if (dirfd <=0) {
        log_error("The file %s could not be opened for monitoring.  Error was %s.", context->folder, strerror(errno));
        return(-1);
    }
   
    /* Set up a list of events to monitor. Changes to the link count should indicate sub-directories created/deleted */
    unsigned int vnode_events = NOTE_LINK;
    struct kevent direvent;
    EV_SET (&direvent, dirfd, EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE, vnode_events, 0, (void *)user_data);
    struct kevent event_data;

    /* NOTE that we do not need a register a signal handler here, kevent() will catch signals for us and return an error code */
    while (1) {
        // camp on kevent() until something interesting happens
        int event_count = kevent(kq, &direvent, 1, &event_data, 1, NULL);
        if ((event_count < 0) || (event_data.flags == EV_ERROR)) {
             /* An error occurred. */
             log_error("An error occurred (event count %d).  The error was %s.", event_count, strerror(errno));
             break;
         }
         if (event_count) {
             context_t *context = (context_t *)(event_data.udata);
             
             log_trace("Event %p occurred.  Filter %d, flags %d, filter flags %s, filter data %p, path %s",
                 (void*)event_data.ident,
                 event_data.filter,
                 event_data.flags,
                 flagstring(event_data.fflags),
                 (void*)event_data.data,
                 context->folder);
             
             process(context);
         } else {
             printf("No event.\n");
         }
    }
    close (kq);
    return 0;
}

int main(int argc, const char * argv[])
{
    context_t context = {
        "/Users/nickblievers/Downloads",
        "/Volumes/Multimedia/Music/FLAC",
        "/Volumes/Multimedia/Music/aac",
        { 0, 0 },
    };
    
    // TODO: Need to read config from file
    
    // TODO: Get command line args for log level
    log_set_level(LOG_TRACE);
    
    log_trace("Using:\n\tWatch Folder:%s\n\tFLAC Folder:%s\n\tAAC Folder:%s", context.folder, context.flac_music_folder, context.aac_music_folder);
    // Pre-process folder before we start watching
    log_trace("Calling pre-watch processing");
    process(&context);
    
    // Call will block waiting for updates
    watch_folder(&context);
    
    return 0;
}
