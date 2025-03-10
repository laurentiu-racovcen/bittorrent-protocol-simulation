#ifndef _UTILS_H_
#define _UTILS_H_

#include <iostream>

#define TRACKER_RANK                  0
#define MAX_FILES                    10
#define MAX_FILENAME                 15
#define HASH_SIZE                    32
#define MAX_SEGMENTS                100
#define MAX_PEERS_NUMBER            100
#define MAX_SEEDS_NUMBER            100
#define MAX_MESSAGE_CONTENT_SIZE     50
#define UPDATE_SEGMENTS_LIMIT        10
#define PEERS_LIST_REQ_NUM            6

/* tracker message codes */
#define CLIENT_REQUEST_CODE                    1
#define FINISHED_DOWNLOAD_FILE_CODE            2
#define FINISHED_ALL_DOWNLOADS_CODE            3
#define ALL_CLIENTS_FINISHED_DOWNLOADING_CODE  4

/* clients message codes */
#define SEGMENT_REQUEST_CODE   5
#define SEGMENT_RESPONSE_CODE  6

enum ClientType {
    PEER,
    SEED
};

using namespace std;

typedef struct {
    char name[MAX_FILENAME];
    unsigned long segments_num;
    char segments_hashes[MAX_SEGMENTS][HASH_SIZE + 1];  // +1 for NULL terminator
} client_file_t;

typedef struct {
    client_file_t files[MAX_FILES];
    unsigned long files_num;
} client_files_t;

typedef struct {
    unsigned int peers_num;
    unsigned int seeds_num;
    int peers[MAX_PEERS_NUMBER];
    int seeds[MAX_SEEDS_NUMBER];
} swarm_info_t;

typedef struct {
    char name[MAX_FILENAME];
    unsigned long segments_num;
    char segments_hashes[MAX_SEGMENTS][HASH_SIZE + 1];  // +1 for NULL terminator
    swarm_info_t swarm;
    int owner;                                          // initial owner rank
} file_info_t;

typedef struct {
    char name[MAX_FILENAME];
    bool received_all_segments;
    unsigned long segments_num;
    char segments_hashes[MAX_SEGMENTS][HASH_SIZE + 1];  // +1 for NULL terminator
    unsigned long received_segments_num;
    bool received_segments[MAX_SEGMENTS];               // received_segments[i] corresponds to segments_hashes[i]
    swarm_info_t swarm;
} wanted_file_info_t;

typedef struct {
    int rank;
    vector<wanted_file_info_t> *wanted_files;
} download_thread_arg;

typedef struct {
    int rank;
    client_files_t *client_files;
} upload_thread_arg;

typedef struct {
    int source_rank;
    int code;
    char content[MAX_MESSAGE_CONTENT_SIZE];
} message_t;

#endif
