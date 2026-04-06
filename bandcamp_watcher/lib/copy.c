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

#define BUFSIZE 128*1024 // in general 128KB should be an efficient blocksize for copying

typedef unsigned char byte_t;

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
    if(dir_exists(src_path) == 0)
    {
        return ENOENT;
    }
    if(dir_exists(dst_path) == 0)
    {
        if(mkdir(dst_path, 755))
        {
            perror(dst_path);
            return errno;
        }
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
            char full_src_path[NAME_MAX+1];
            sprintf(full_src_path, "%s/%s", src_path, de->d_name);
            char full_dst_path[NAME_MAX+1];
            sprintf(full_dst_path, "%s/%s", dst_path, de->d_name);

            copy(full_src_path,full_dst_path);
        }
    }
    
    (void)closedir(d);
        
    return 0;
}

