#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string.h>
#include <unistd.h>
#include <set>

#include "utils.h"

using namespace std;

/* Function that sends a non-received segment request message
 * to a random peer/seed, and then receives the requested segment */
bool send_next_segment_request(int rank, wanted_file_info_t *wanted_file, ClientType list_type) {
    message_t send_message;
    send_message.code = SEGMENT_REQUEST_CODE;
    send_message.source_rank = rank;
    strcpy(send_message.content, wanted_file->
            segments_hashes[wanted_file->received_segments_num]);

    /* for efficiency purposes, every segment request is
     * for a random peer/seed (different from the source client),
     * to not overload a certain peer/seed */
    int random_client_index;
    int dest_client_rank;
    if (list_type == PEER) {
        random_client_index = rand() % wanted_file->swarm.peers_num;
        while (rank == wanted_file->swarm.peers[random_client_index]) {
            random_client_index = rand() % wanted_file->swarm.peers_num;
        }
        dest_client_rank = wanted_file->swarm.peers[random_client_index];
    } else {
        random_client_index = rand() % wanted_file->swarm.seeds_num;
        while (rank == wanted_file->swarm.seeds[random_client_index]) {
            random_client_index = rand() % wanted_file->swarm.seeds_num;
        }
        dest_client_rank = wanted_file->swarm.seeds[random_client_index];
    }

    /* send segment request to the found client */
    MPI_Send(&send_message, sizeof(message_t), MPI_BYTE, dest_client_rank,
            dest_client_rank, MPI_COMM_WORLD);

    /* receive the file segment from dest_client_rank
    * (actually, the "ACK" message of segment) */
    MPI_Status recv_status;
    message_t recv_message;
    MPI_Recv(&recv_message, sizeof(message_t), MPI_BYTE, dest_client_rank,
        dest_client_rank, MPI_COMM_WORLD, &recv_status);

    /* check if the hash is the same as requested + ACK string */
    if (recv_message.code == SEGMENT_RESPONSE_CODE
        && strcmp(recv_message.content, wanted_file->
                segments_hashes[wanted_file->received_segments_num]) == 0
        && strcmp(recv_message.content + HASH_SIZE + 1, "ACK") == 0)
    {
        /* mark segment as received */
        wanted_file->received_segments[wanted_file->received_segments_num] = true;
        wanted_file->received_segments_num++;
        return true;
    }

    return false;
}

/* Function that requests from tracker all
 * the wanted files swarms and stores them */
void get_swarms_from_tracker(int client_rank, vector<wanted_file_info_t> *client_wanted_files) {
    for (size_t i = 0; i < client_wanted_files->size(); i++) {
        /* generate request message */
        message_t message;
        message.code = CLIENT_REQUEST_CODE;
        message.source_rank = client_rank;
        strcpy(message.content, client_wanted_files->at(i).name);

        MPI_Send(&message, sizeof(message_t), MPI_BYTE, TRACKER_RANK,
                TRACKER_RANK, MPI_COMM_WORLD);
        
        /* receive file swarm from tracker */
        file_info_t recv_file_info;
        MPI_Status status;
        MPI_Recv(&recv_file_info, sizeof(file_info_t), MPI_BYTE, TRACKER_RANK,
                TRACKER_RANK, MPI_COMM_WORLD, &status);

        if (client_wanted_files->at(i).segments_num == 0) {
            /* if it's the first swarm message from tracker,
             * also copy the segments hashes */
            memcpy(&client_wanted_files->at(i).swarm, &recv_file_info.swarm,
                  sizeof(swarm_info_t));
            memcpy(client_wanted_files->at(i).segments_hashes,
                  recv_file_info.segments_hashes, MAX_SEGMENTS*(HASH_SIZE+1));
            client_wanted_files->at(i).segments_num = recv_file_info.segments_num;
        } else {
            /* update only the swarm of the wanted file */
            memcpy(&client_wanted_files->at(i).swarm, &recv_file_info.swarm,
                  sizeof(swarm_info_t));
        }
    }
}

void *download_thread_func(void *arg) {
    download_thread_arg *download_arg = (download_thread_arg*) arg;
    vector<wanted_file_info_t> *wanted_files = download_arg->wanted_files;
    unsigned int segments_downloaded_num = 0;
    unsigned int requests_counter = 0;

    /* get swarm lists of client's wanted files from tracker */
    get_swarms_from_tracker(download_arg->rank, wanted_files);

    while (true) {
        /* search for unreceived files segments */
        for (size_t i = 0; i < wanted_files->size(); i++) {
            wanted_file_info_t *wanted_file = &wanted_files->at(i);
            if (wanted_file->received_segments_num < wanted_file->segments_num) {
                ClientType list_type;
                /* choose the list (seeds/peers)
                 * where to search for the next client */
                if (requests_counter < PEERS_LIST_REQ_NUM) {
                    if (wanted_file->swarm.peers_num != 0) {
                        list_type = PEER;
                    } else {
                        list_type = SEED;
                    }
                } else {
                    if (wanted_file->swarm.seeds_num != 0) {
                        list_type = SEED;
                    } else {
                        list_type = PEER;
                    }
                }
                
                bool segment_is_downloaded =
                    send_next_segment_request(download_arg->rank, wanted_file, list_type);
                if (segment_is_downloaded == true) {
                    segments_downloaded_num++;
                }
                requests_counter++;
                if (requests_counter == 10) {
                    /* reset requests counter */
                    requests_counter = 0;
                }
            } else if (wanted_files->at(i).received_all_segments == false) {
                /* file's received_segments_num == file's segments_num,
                 * send finished file processing message to tracker */
                message_t send_message;
                send_message.code = FINISHED_DOWNLOAD_FILE_CODE;
                send_message.source_rank = download_arg->rank;
                strcpy(send_message.content, wanted_file->name);

                /* send segment request to found client */
                MPI_Send(&send_message, sizeof(message_t), MPI_BYTE,
                        TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

                wanted_file->received_all_segments = true;

                /* write output file of finished file */
                string output_file_name = "client" +
                    to_string(download_arg->rank) + "_" + wanted_file->name;
                ofstream fout(output_file_name);

                for (size_t i = 0; i < wanted_file->received_segments_num; i++) {
                    fout << wanted_file->segments_hashes[i] << "\n";
                }

                fout.close();
            }

            if (segments_downloaded_num == UPDATE_SEGMENTS_LIMIT) {
                /* 10 segments have been downloaded,
                 * update the swarm of all files and reset the "for" loop */
                get_swarms_from_tracker(download_arg->rank, wanted_files);
                segments_downloaded_num = 0;
                i = 0;
            }
        }

        unsigned int received_files_count = 0;
        for (size_t i = 0; i < wanted_files->size(); i++) {
            if (wanted_files->at(i).received_all_segments == true) {
                received_files_count++;
            }
        }

        if (received_files_count == wanted_files->size()) {
            /* the client has received the segments of all files,
             * send a completion message to the tracker, and exit */
            message_t send_message;
            send_message.code = FINISHED_ALL_DOWNLOADS_CODE;
            send_message.source_rank = download_arg->rank;

            /* send segment request to the found client */
            MPI_Send(&send_message, sizeof(message_t), MPI_BYTE,
                    TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

            pthread_exit(NULL);
        }
    }

    return NULL;
}

/* Function that generates an "ACK" or "NACK" response message for a requested segment:
 * "ACK" - if the requested file segment exists in the client's files
 * "NACK" - if the requested file segment doesn't exist in the client's files */
message_t get_file_segment(char* requested_segment_hash, int rank, client_files_t *client_files) {
    message_t response_message;
    response_message.code = SEGMENT_RESPONSE_CODE;
    response_message.source_rank = rank;
    strcpy(response_message.content, requested_segment_hash);

    for (size_t i = 0; i < client_files->files_num; i++) {
        for (size_t j = 0; j < client_files->files[i].segments_num; j++) {
            if (strcmp(requested_segment_hash,
                        client_files->files[i].segments_hashes[j]) == 0) {
                /* return the requested file segment
                 * (actually, an "ACK" message of the requested segment) */
                strcpy(response_message.content + HASH_SIZE + 1, "ACK");

                return response_message;
            }
        }
    }

    /* the requested file segment does not exist, return "NACK" message */
    strcpy(response_message.content + HASH_SIZE + 1, "NACK");

    return response_message;
}

void *upload_thread_func(void *arg) {
    upload_thread_arg *upload_arg = (upload_thread_arg*) arg;

    /* respond to segment requests from other clients,
    for segments that this client has */
    while (true) {
        message_t client_message;
        MPI_Status status;
        MPI_Recv(&client_message, sizeof(message_t), MPI_CHAR, MPI_ANY_SOURCE,
                upload_arg->rank, MPI_COMM_WORLD, &status);

        if (client_message.code == SEGMENT_REQUEST_CODE) {
            message_t response_message = get_file_segment(client_message.content,
                                    upload_arg->rank, upload_arg->client_files);

            MPI_Send(&response_message, sizeof(message_t), MPI_CHAR,
                    status.MPI_SOURCE, upload_arg->rank, MPI_COMM_WORLD);
        }
        else if (client_message.code == ALL_CLIENTS_FINISHED_DOWNLOADING_CODE) {
            /* stop thread execution */
            pthread_exit(NULL);
        }
    }

    return NULL;
}

/* Function that returns the index of a searched file,
 * or "-1" if the file is not in the tracker's file list */
int get_tracked_file_index(char* filename, vector<file_info_t>* files_info) {
    for (size_t i = 0; i < files_info->size(); i++) {
        if (strcmp(files_info->at(i).name, filename) == 0) {
            return (int) i;
        }
    }

    return -1;
}

void tracker(int numtasks, int rank) {
    vector<file_info_t> files_info;
    /* initial clients' messages processing */
    for (int i = 1; i < numtasks; i++) {
        MPI_Status status;
        client_files_t client_files;
        MPI_Recv(&client_files, sizeof(client_files_t), MPI_BYTE, i, i,
                MPI_COMM_WORLD, &status);

        for (size_t j = 0; j < client_files.files_num; j++) {
            /* store client's current file info */
            file_info_t current_file_info;
            strcpy(current_file_info.name, client_files.files[j].name);
            current_file_info.segments_num = client_files.files[j].segments_num;
            current_file_info.owner = i;

            for (size_t k = 0; k < client_files.files[j].segments_num; k++) {
                strcpy(current_file_info.segments_hashes[k],
                        client_files.files[j].segments_hashes[k]);
            }

            /* add source client rank in the swarm list */
            for (size_t k = 0; k < current_file_info.segments_num; k++) {
                current_file_info.swarm.peers_num = 1;
                current_file_info.swarm.peers[0] = i;
            }

            files_info.push_back(current_file_info);
        }
    }

    /* all clients have sent the initial message,
     * send a confirmation message to all the clients */
    string ack_message = "ACK";
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(ack_message.c_str(), ack_message.size() * sizeof(char) + 1,
                MPI_CHAR, i, rank, MPI_COMM_WORLD);
    }

    set<int> finished_clients;

    /* clients' messages processing */
    while (true) {
        message_t client_message;
        MPI_Status status;
        MPI_Recv(&client_message, sizeof(message_t), MPI_BYTE, MPI_ANY_SOURCE,
                TRACKER_RANK, MPI_COMM_WORLD, &status);

        /* check the request code */
        switch (client_message.code) {
            case CLIENT_REQUEST_CODE: {
                int file_index = get_tracked_file_index(client_message.content, &files_info);
                if (file_index != -1) {
                    /* the tracker has the requested file */

                    /* send the file info containing the swarm
                     * of the requested file to the client */
                    MPI_Send(&files_info[file_index], sizeof(file_info_t),
                        MPI_BYTE, client_message.source_rank, TRACKER_RANK, MPI_COMM_WORLD);

                    /* add source client to the peers list */
                    bool is_source_client_in_list = false;
                    for (size_t i = 0; i < files_info[file_index].swarm.peers_num; i++) {
                        if (files_info[file_index].swarm.peers[i] == client_message.source_rank) {
                            /* source rank is already in the peer list, no need to add it */
                            is_source_client_in_list = true;
                            break;
                        }
                    }

                    if (is_source_client_in_list == false) {
                        files_info[file_index].swarm.peers[files_info[file_index].swarm.peers_num] =
                            client_message.source_rank;
                        files_info[file_index].swarm.peers_num++;
                    }
                }
                break;
            }
            case FINISHED_DOWNLOAD_FILE_CODE: {
                int file_index = get_tracked_file_index(client_message.content, &files_info);
                if (file_index != -1) {
                    /* remove source client from the peers list, if it's here */
                    for (size_t i = 0; i < files_info[file_index].swarm.peers_num; i++) {
                        if (files_info[file_index].swarm.peers[i] == client_message.source_rank) {
                            /* source rank is in the peer list, remove it */

                            /* move elements to the left by one place */
                            for (size_t j = i; j < files_info[file_index].swarm.peers_num-1; j++) {
                                files_info[file_index].swarm.peers[j] =
                                    files_info[file_index].swarm.peers[j+1];
                            }
                            
                            files_info[file_index].swarm.peers_num--;
                            break;
                        }
                    }

                    /* add source client to the seeds list */
                    files_info[file_index].swarm.seeds[files_info[file_index].swarm.seeds_num]
                        = client_message.source_rank;
                    files_info[file_index].swarm.seeds_num++;
                }
                break;
            }
            case FINISHED_ALL_DOWNLOADS_CODE: {
                /* mark client as finished */
                finished_clients.insert(client_message.source_rank);
                if ((int)finished_clients.size() == numtasks-1) {
                    /* send all clients finished message,
                     * in order to finish execution of all clients */
                    message_t response_message;
                    response_message.code = ALL_CLIENTS_FINISHED_DOWNLOADING_CODE;
                    response_message.source_rank = rank;
                    for (int i = 1; i < numtasks; i++) {
                        MPI_Send(&response_message, sizeof(message_t),
                                MPI_BYTE, i, i, MPI_COMM_WORLD);
                    }
                    /* exit tracker process */
                    return;
                }
                break;
            }
            default:
                break;
        }
    }
}

/* Function that reads a client file and stores its data */
void get_client_data(int rank,
                    client_files_t *client_files,
                    vector<wanted_file_info_t> *wanted_files)
{
    /* read peer's file */
    ifstream fin("in" + to_string(rank) + ".txt");

    /* read peer's files number */
    string line;
    getline(fin, line);
    client_files->files_num = stoul(line);

    /* read peer's files details */
    for (size_t i = 0; i < client_files->files_num; i++) {
        /* read current file's details */
        string file_details_line;
        getline(fin, file_details_line);
        stringstream file_details_stream(file_details_line);

        /* extract current file name */
        string temp_name;
        file_details_stream >> temp_name;
        strcpy(client_files->files[i].name, temp_name.c_str());

        /* extract current file segments number */
        string segments_num_string;
        file_details_stream >> segments_num_string;
        client_files->files[i].segments_num = stoul(segments_num_string);

        /* extract current file segments */
        for (size_t j = 0; j < client_files->files[i].segments_num; j++) {
            string current_segment_hash;
            getline(fin, current_segment_hash);
            strcpy(client_files->files[i].segments_hashes[j],
                current_segment_hash.c_str());
        }
    }

    /* read peer's wanted files names */
    getline(fin, line);
    unsigned long wanted_files_num = stoul(line);

    /* read peer's wanted files' names */
    for (size_t i = 0; i < wanted_files_num; i++) {
        /* read current file's name */
        wanted_file_info_t current_wanted_file;
        string current_file_name;

        getline(fin, current_file_name);

        /* initialize current wanted file fields */
        strcpy(current_wanted_file.name, current_file_name.c_str());
        current_wanted_file.segments_num = 0;
        current_wanted_file.received_all_segments = false;
        current_wanted_file.received_segments_num = 0;

        for (size_t i = 0; i < MAX_SEGMENTS; i++) {
            current_wanted_file.received_segments[i] = false;
        }
        
        wanted_files->push_back(current_wanted_file);
    }
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    /* initialize files data */
    client_files_t client_files;
    vector<wanted_file_info_t> wanted_files;

    get_client_data(rank, &client_files, &wanted_files);

    MPI_Send(&client_files, sizeof(client_files_t), MPI_BYTE, TRACKER_RANK,
            rank, MPI_COMM_WORLD);

    /* wait for the confirmation message from tracker */
    MPI_Status recv_status;
    char confirmation[4];
    MPI_Recv(&confirmation, sizeof(confirmation), MPI_BYTE, TRACKER_RANK,
        TRACKER_RANK, MPI_COMM_WORLD, &recv_status);

    if (strcmp(confirmation, "ACK") != 0) {
        exit(-2);
    }

    download_thread_arg download_arg;
    download_arg.rank = rank;
    download_arg.wanted_files = &wanted_files;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &download_arg);
    if (r) {
        printf("Error creating the download thread.\n");
        exit(-1);
    }

    upload_thread_arg upload_arg;
    upload_arg.rank = rank;
    upload_arg.client_files = &client_files;

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &upload_arg);
    if (r) {
        printf("Error creating the upload thread.\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Error waiting for the download thread.\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Error waiting for the upload thread.\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI does not support multi-threading.\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
