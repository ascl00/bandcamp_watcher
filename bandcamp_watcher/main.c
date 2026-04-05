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
#include <limits.h>

#include "log.h"    // logging functions
#include "copy.h"
#include "utils.h"
#include "bandcamp.h"

typedef struct {
    const char *folder; // folder to watch for new directories to be created in
    const char *flac_music_folder; // folder to copy FLAC files
    const char *aac_music_folder; // folder to copy AAC files
    struct timeval last_run;        // last time we processed an event (if ever)
} context_t;

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
