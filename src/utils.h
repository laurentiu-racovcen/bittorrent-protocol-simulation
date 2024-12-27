#ifndef _UTILS_H_
#define _UTILS_H_

#include <iostream>
#include <set>

#define TRACKER_RANK          0
#define MAX_FILES            10
#define MAX_FILENAME         15
#define HASH_SIZE            32
#define MAX_SEGMENTS        100
#define MAX_CLIENTS_NUMBER  100
#define MAX_MESSAGE_SIZE     50

/* tracker message codes */
#define CLIENT_REQUEST_CODE                    1
#define FINISHED_DOWNLOAD_FILE_CODE            2
#define FINISHED_ALL_DOWNLOADS_CODE            3
#define ALL_CLIENTS_FINISHED_DOWNLOADING_CODE  4

/* clients message codes */
#define SEGMENT_REQUEST_CODE   5
#define SEGMENT_RESPONSE_CODE  6

#define UPDATE_SEGMENTS_LIMIT 10

/* client types */
enum ClientType {
    PEER,
    SEED
};

using namespace std;

typedef struct {
    char name[MAX_FILENAME];
    unsigned long segments_num;
    char segments_hashes[MAX_SEGMENTS][HASH_SIZE + 1]; // +1 for NULL terminator
} client_file_t;

typedef struct {
    client_file_t files[MAX_FILES];
    unsigned long files_num;
} client_files_t;

typedef struct {
    int rank;
    enum ClientType type;
} client_info_t;

typedef struct {
    unsigned int clients_num;
    client_info_t clients[MAX_CLIENTS_NUMBER];
} swarm_info_t;

typedef struct {
    char name[MAX_FILENAME];
    unsigned long segments_num;
    char segments_hashes[MAX_SEGMENTS][HASH_SIZE + 1]; // +1 for NULL terminator
    swarm_info_t segments_swarms[MAX_SEGMENTS];        // segments_swarms[i] corresponds to segment_hashes[i]
    int owner;
} file_info_t;

typedef struct {
    char name[MAX_FILENAME];
    unsigned long segments_num;
    unsigned long received_segments_num;
    bool received_all_segments;
    char segments_hashes[MAX_SEGMENTS][HASH_SIZE + 1]; // +1 for NULL terminator
    bool received_segments[MAX_SEGMENTS];              // if received_segments[i] == true, the file segment with index "i" has been received by the client
    swarm_info_t segments_swarms[MAX_SEGMENTS];        // segments_swarms[i] corresponds to segment_hashes[i]
} wanted_file_info_t;

typedef struct {
    int rank;
    vector<wanted_file_info_t> *wanted_files;
} download_thread_arg;

typedef struct {
    int source_rank;
    int code;
    char content[MAX_MESSAGE_SIZE];
} message_t;

#endif
