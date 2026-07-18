//
//  copy.c
//  bandcamp_watcher
//
//  Created by Nick Blievers on 21/6/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>    // for stat()
#include <string.h>
#include <dirent.h>
#include <sys/syslimits.h>
#include <unistd.h>

#include "utils.h"

#define BUFSIZE 128*1024 // in general 128KB should be an efficient blocksize for copying

typedef unsigned char byte_t;

static void remove_partial_clone(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;
        char file_path[PATH_MAX];
        if (path_join(file_path, sizeof(file_path), path, entry->d_name) == 0) {
            (void)unlink(file_path);
        }
    }
    (void)closedir(dir);
    (void)rmdir(path);
}

int dir_exists(const char* path)
{
    struct stat s;
    
    int ret = stat(path, &s);
    
    if(ret == 0 && s.st_mode & S_IFDIR)
    {
        return 1;
    }
    
    return 0;
}

int filecopy(FILE *src, FILE *dst)
{
    byte_t buf[BUFSIZE];
    size_t numread, numwrite;
    while ((numread = fread(buf, 1, BUFSIZE, src)) > 0)
    {
        if(ferror(src))
        {
            break;
        }
        numwrite = fwrite(buf, 1, numread, dst);
        if (numwrite != numread)
        {
            fputs("Write error or mismatch!\n", stderr);
            // Only clear error if there was one
            if (ferror(dst))
                clearerr(dst);
            return 1;
        }
        if(feof(src))
            break;
    }
    // After loop ends, check why
    if (ferror(src))
    {
        fputs("Read error!\n", stderr);
        clearerr(src);
        return 2;
    }
    // If feof(src), it's normal end of file, no action needed
    return 0;
}

int copy(const char* src_file_name, const char* dst_file_name)
{
    FILE *s, *d;
    int err = 0;
    
    s = fopen(src_file_name, "rb");
    if (s == NULL)
    {
        perror(src_file_name);
        return errno;
    }

    d = fopen(dst_file_name, "wb");
    if (d == NULL)
    {
        perror(dst_file_name);
        err = errno;
        fclose(s);
        return err;
    }

    int copy_err = filecopy(s, d);
    fclose(s);
    fclose(d);
    if (copy_err != 0)
    {
        fprintf(stderr, "Error copying file %s to %s (code %d)\n", src_file_name, dst_file_name, copy_err);
        return copy_err;
    }
    return 0;
}

int clone(const char *src_path, const char *dst_path)
{
    int created_destination = 0;
    if(dir_exists(src_path) == 0)
    {
        return ENOENT;
    }
    if(dir_exists(dst_path) == 0)
    {
        if(mkdir(dst_path, 0755))
        {
            perror(dst_path);
            return errno;
        }
        created_destination = 1;
    }
    DIR *d = opendir(src_path);
    
    if(!d)
    {
        perror(src_path);
        return errno;
    }
    struct dirent *de = NULL;
    
    while ( (de = readdir(d)) != NULL)
    {
        if (de->d_type == DT_REG)
        {
            char full_src_path[PATH_MAX];
            char full_dst_path[PATH_MAX];
            if (path_join(full_src_path, sizeof(full_src_path), src_path, de->d_name) != 0 ||
                path_join(full_dst_path, sizeof(full_dst_path), dst_path, de->d_name) != 0) {
                (void)closedir(d);
                if (created_destination) remove_partial_clone(dst_path);
                return ENAMETOOLONG;
            }

            int copy_error = copy(full_src_path, full_dst_path);
            if (copy_error != 0) {
                (void)unlink(full_dst_path);
                (void)closedir(d);
                if (created_destination) remove_partial_clone(dst_path);
                return copy_error;
            }
        }
    }
    
    (void)closedir(d);
        
    return 0;
}
