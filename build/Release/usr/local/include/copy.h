//
//  copy.h
//  bandcamp_watcher
//
//  Created by Nick on 21/6/2025.
//
#ifndef COPY_H
#define COPY_H
// Takes the contents of src_file_name and writes them into dst_file_name
int copy(const char* src_file_name, const char* dst_file_name);

// Takes the contents of the src_path, and re-creates them inside dst_path.
// This will create the directory dst_path if it doesn't already exist.
int clone(const char *src_path, const char *dst_path);

// Returns non-zero if the path exists and is a directory
int dir_exists(const char* path);
#endif
