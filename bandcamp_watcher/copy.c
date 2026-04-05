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

void filecopy(FILE *src, FILE *dst)
{
   byte_t buf[BUFSIZE];
   size_t numread, numwrite;

   while (!feof(src)) {
       numread = fread(buf, sizeof(byte_t), BUFSIZE, src);

       if (numread > 0) {
           numwrite = fwrite(buf, sizeof(byte_t), numread, dst);

           if (numwrite != numread) {
               fputs("mismatch!\n", stderr);
               return;
           }
       }
   }
}

int copy(const char* src_file_name, const char* dst_file_name)
{
    FILE *s, *d;
    int err = 0;
    
    s = fopen(src_file_name, "rb");
    if (s == NULL) {
        perror(src_file_name);
        return errno;
    }

    d = fopen(dst_file_name, "wb");
    if (d == NULL) {
        perror(dst_file_name);
        err = errno;
        fclose(s);
        return err;
    }

    filecopy(s, d);
    fclose(s);
    fclose(d);
    
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
