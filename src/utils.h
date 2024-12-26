#ifndef _UTILS_H_
#define _UTILS_H_

#include <iostream>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_SEGMENTS 100

using namespace std;

typedef struct {
    char name[MAX_FILENAME];
    unsigned long segments_num;
    char segments[MAX_SEGMENTS][HASH_SIZE + 1]; // +1 for NULL terminator
} client_file_t;

typedef struct {
    client_file_t files[MAX_FILES];
    unsigned long files_num;
} client_files_t;

typedef struct {
    char name[MAX_FILENAME];
    unsigned long segments_num;
    char segments[MAX_SEGMENTS][HASH_SIZE + 1]; // +1 for NULL terminator
    vector<int> swarm; // vector of client ranks that have segments containing this file
} file_info_t;

#endif
