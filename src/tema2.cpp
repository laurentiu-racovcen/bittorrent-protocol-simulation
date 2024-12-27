#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string.h>
#include <unistd.h>

#include "utils.h"

using namespace std;

/* Function that requests from tracker all
 * the wanted files swarms and stores them */
void get_swarms_from_tracker(int client_rank, vector<wanted_file_info_t> *client_wanted_files) {
    cout << "! client " << client_rank << ": wanted files nr: " << client_wanted_files->size() << "\n";
    /* request all wanted files swarms from tracker */
    for (size_t i = 0; i < client_wanted_files->size(); i++) {
        cout << "client " << client_rank << ": sending to tracker a file request of file: " << client_wanted_files->at(i).name << "\n";

        /* generate request message */
        message_t message;
        message.code = CLIENT_REQUEST_CODE;
        message.source_rank = client_rank;
        strcpy(message.content, client_wanted_files->at(i).name);

        MPI_Send(&message, sizeof(message_t), MPI_CHAR, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);
        cout << "\nclient " << client_rank << ": sent request message of file " << client_wanted_files->at(i).name << "\n";

        /* receive file swarm from tracker */
        file_info_t recv_file_info;
        MPI_Status status;
        MPI_Recv(&recv_file_info, sizeof(file_info_t), MPI_BYTE, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD, &status);

        /* update current wanted file info */
        memcpy(&client_wanted_files->at(i).segments_swarms, &recv_file_info.segments_swarms, sizeof(swarm_info_t));

        /* if it's the first swarm message from tracker,
         * copy the segments hashes */
        if (client_wanted_files->at(i).segments_num == 0) {
            memcpy(client_wanted_files->at(i).segments_hashes, recv_file_info.segments_hashes, MAX_SEGMENTS*(HASH_SIZE+1));
            client_wanted_files->at(i).segments_num = recv_file_info.segments_num;
        }

        cout << "\nclient " << client_rank
            << ": received from tracker the swarm of file " << recv_file_info.name
            << ", last seg hash = " << client_wanted_files->at(i).segments_hashes[client_wanted_files->at(i).segments_num-1]
            << "\n";
    }
}

/* Function that sends a non-received segment request message to a peer/seed,
 * and then receives the requested segment */

void send_next_segment_request(int rank, wanted_file_info_t *wanted_file) {
    /* generate file segment request message for peer/seed */
    message_t send_message;
    send_message.code = SEGMENT_REQUEST_CODE;
    send_message.source_rank = rank;
    strcpy(send_message.content, wanted_file->segments_hashes[wanted_file->received_segments_num]);

    /* for efficiency purposes, every segment request is
     * for a random peer/seed, to not overload a certain peer/seed */

    //  cout << "clients number: " << wanted_file->segments_swarms[wanted_file->received_segments_num].clients_num;
    int random_client_index = rand() % wanted_file->segments_swarms[wanted_file->received_segments_num].clients_num;
    int dest_client_rank = wanted_file->segments_swarms[wanted_file->received_segments_num].clients[random_client_index].rank;

    /* send segment request to found client */
    MPI_Send(&send_message, sizeof(message_t), MPI_CHAR, dest_client_rank, rank, MPI_COMM_WORLD);

    cout << "\nclient " << rank
        << " -> client " << dest_client_rank
        << ": sent segment " << wanted_file->received_segments_num
        << " (" << wanted_file->segments_hashes[wanted_file->received_segments_num]
        << ") request message of file " << wanted_file->name << "\n";

    /* receive the file segment from dest_client_rank
     * (actually, the "ACK" message of segment) */
    MPI_Status recv_status;
    message_t recv_message;
    MPI_Recv(&recv_message, sizeof(message_t), MPI_BYTE, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD, &recv_status);

    /* check if the hash is the same as requested + ACK string */
    if (recv_message.code != SEGMENT_RESPONSE_CODE
     || strcmp(recv_message.content, wanted_file->segments_hashes[wanted_file->received_segments_num]) != 0
     || strcmp(recv_message.content + HASH_SIZE + 1, "ACK") != 0) {
        exit(-2);
    }

    cout << "client " << rank << " received hash ack: " << wanted_file->segments_hashes[wanted_file->received_segments_num] << "\n";

    /* mark segment as received */
    wanted_file->received_segments[wanted_file->received_segments_num] = true;

    /* increment received segments number */
    wanted_file->received_segments_num++;
}

void *download_thread_func(void *arg) {
    download_thread_arg *download_arg = (download_thread_arg*) arg;
    vector<wanted_file_info_t> *wanted_files = download_arg->wanted_files;
    unsigned int received_files_count = 0;
    unsigned int segments_downloaded_num = 0;

    /* get swarm lists of client's wanted files from tracker */
    get_swarms_from_tracker(download_arg->rank, wanted_files);

    cout << "\n,,, client " << download_arg->rank << ": number of segments of first file: " << download_arg->wanted_files->at(0).segments_num << "\n";

    while (true) {
        /* search for unreceived files segments */
        for (size_t i = 0; i < wanted_files->size(); i++) {
            if (wanted_files->at(i).received_segments_num < wanted_files->at(i).segments_num) {
                send_next_segment_request(download_arg->rank, &wanted_files->at(i));
                segments_downloaded_num++;
            } else if (wanted_files->at(i).received_all_segments == false) {
                /* file's received_segments_num == file's segments_num,
                 * send finished file processing message to tracker */

                // TODO: send to tracker finished file message
                wanted_files->at(i).received_all_segments = true;
                received_files_count++;
            }

            if (segments_downloaded_num == UPDATE_SEGMENTS_LIMIT) {
                /* 10 segments have been downloaded,
                 * update the swarm of all files and reset the "for" loop */
                get_swarms_from_tracker(download_arg->rank, wanted_files);
                segments_downloaded_num = 0;
                i = 0;
            }
        }

        if (received_files_count == wanted_files->size()) {
            /* the client has received the segments of all files,
             * send a completion message to the tracker, and exit */

            // TODO:
        }
    }

    /* TODO: after 10 downloaded segments,
       request from tracker the updated swarm list of all the wanted files
    */

    /* TODO: received a request from a client.
        check if it has the requested segment.
        if it does, send "ACK" message;
        if it doesn't, send a negative message.
     */

    return NULL;
}

void *upload_thread_func(void *arg) {
    int rank = *(int*) arg;

    /* TODO: respond to segment requests from from other clients,
    for segments that this client has */

    return NULL;
}

void tracker(int numtasks, int rank) {
    vector<file_info_t> files_info;
    // cout << "tracker rank: "<< rank << "\n";

    /* initial clients' messages processing */
    for (int i = 1; i < numtasks; i++) {
        MPI_Status status;
        client_files_t client_files;
        // cout << "\ntracker: receiving message...\n";

        MPI_Recv(&client_files, sizeof(client_files_t), MPI_BYTE, i, i, MPI_COMM_WORLD, &status);
        // cout << "\ntracker: message received from client " << i
        //     << "\n first file name is: " << client_files.files[0].name
        //     << "\n first file num seg: " << client_files.files[0].segments_num
        //     << "\n files number: " << client_files.files_num
        //     << "\n first seg: " << client_files.files[0].segments_hashes[0] 
        //     << "\n last seg: " << client_files.files[client_files.files_num-1].segments_hashes[client_files.files[client_files.files_num-1].segments_num-1]
        //     << "\n tracker: status received bytes number: " << status._ucount << "\n";

        for (size_t j = 0; j < client_files.files_num; j++) {
            /* store client's current file info */
            file_info_t current_file_info;
            strcpy(current_file_info.name, client_files.files[j].name);
            current_file_info.segments_num = client_files.files[j].segments_num;

            for (size_t k = 0; k < client_files.files[j].segments_num; k++) {
                strcpy(current_file_info.segments_hashes[k], client_files.files[j].segments_hashes[k]);
            }

            current_file_info.owner = i;
            for (size_t k = 0; k < current_file_info.segments_num; k++) {
                current_file_info.segments_swarms[k].clients_num = 1; // source client
                current_file_info.segments_swarms[k].clients[0].rank = i;
                current_file_info.segments_swarms[k].clients[0].type = PEER;
            }

            files_info.push_back(current_file_info);
        }
    }

    // cout << "tracked files and their owners: \n";
    // for (size_t i = 0; i < files_info.size(); i++) {
    //     cout << files_info[i].name << ", "
    //     << files_info[i].owner << ", seg num = "
    //     << files_info[i].segments_num << ", "
    //     << files_info[i].segments_hashes[0] << ", "
    //     << files_info[i].segments_hashes[files_info[i].segments_num-1] << "\nswarm size = " << files_info[i].swarm.seeds_num << " swarm: ";
    //     for (size_t j = 0; j < files_info[i].swarm.seeds_num; j++) {
    //         cout << files_info[i].swarm.seeds[j] << " ";
    //     }
    //     cout << "\n";
    // }

    /* all clients have sent the initial message,
     * send a confirmation message to all the clients */
    string ack_message = "ACK";
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(ack_message.c_str(), ack_message.size() * sizeof(char) + 1, MPI_CHAR, i, rank, MPI_COMM_WORLD);
    }

    cout << "tracker: the confirmation of the initial message has been sent to all the clients!\n";

    /* wait for all clients to receive the "ACK" confirmation message */
    MPI_Barrier(MPI_COMM_WORLD);
    cout << "\nbarrier unlocked! - all the clients have received the initial message confirmation\n";

    /* clients' messages processing */
    while (true) {
        message_t client_message;
        MPI_Status status;
        MPI_Recv(&client_message, sizeof(message_t), MPI_CHAR, MPI_ANY_SOURCE, TRACKER_RANK, MPI_COMM_WORLD, &status);

        switch (client_message.code) {
            case CLIENT_REQUEST_CODE: {

                cout << "tracker: received client request message from " << client_message.source_rank
                        << " with file: " << client_message.content << "\n";

                /* check if the tracker tracks the requested file */
                int file_index = -1;
                for (size_t i = 0; i < files_info.size(); i++) {
                    if (strcmp(files_info[i].name, client_message.content) == 0) {
                        file_index = (int) i;
                        break;
                    }
                }

                if (file_index >= 0) {
                    /* the tracker has this file, send the swarm of the requested file to the client */
                    MPI_Send(&files_info[file_index], sizeof(file_info_t), MPI_BYTE, client_message.source_rank, TRACKER_RANK, MPI_COMM_WORLD);
                    cout << "tracker: sent swarm for file " << files_info[file_index].name << " to client " << client_message.source_rank << "!\n";
                } else {
                    cout << "tracker: does not have the file: " << client_message.content << "\n";
                }
                break;
            }
            case FINISHED_DOWNLOAD_FILE_CODE: {
                /* code */
                break;
            }
            case FINISHED_ALL_DOWNLOADS_CODE: {
                /* code */
                break;
            }
            case ALL_CLIENTS_FINISHED_DOWNLOADING_CODE: {
                /* code */
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

    /* initialize files data */
    cout << "peer files nr = " + to_string(client_files->files_num) + ", peer rank = " + to_string(rank) + "\n";

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
            strcpy(client_files->files[i].segments_hashes[j], current_segment_hash.c_str());
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

    // cout << "files struct size = " << sizeof(client_files_t) << "\n";

    // for (size_t i = 0; i < client_files.files_num; i++) {
    //     cout << "file name: " << client_files.files[i].name << "\n";
    //     cout << "file segments number: " << client_files.files[i].segments_num << "\n";
    //     cout << "segments:\n";

    //     /* add current file segments and increment the pointer */
    //     for (size_t j = 0; j < client_files.files[i].segments_num; j++) {
    //         cout << client_files.files[i].segments_hashes[j] << "\n";
    //     }
    // }

    // cout << "wanted file names:\n";
    // for (size_t i = 0; i < wanted_files.size(); i++) {
    //     cout << wanted_files[i].name << "\n";
    // }

    /* send this client's files data (file names and segments) to the tracker */
    // cout << "send buffer size: " << sizeof(client_files_t) << "\n";
    // cout << "sending all the files: " << "\n";
    MPI_Send(&client_files, sizeof(client_files_t), MPI_BYTE, TRACKER_RANK, rank, MPI_COMM_WORLD);
    // cout << "\nclient: message sent! \n";

    /* wait for the confirmation message from tracker */
    MPI_Status recv_status;
    char confirmation[4];
    MPI_Recv(&confirmation, sizeof(confirmation), MPI_BYTE, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD, &recv_status);

    if (strcmp(confirmation, "ACK") != 0) {
        exit(-2);
    }

    cout << "client " << rank << " received initial confirmation! confirmation message: " << confirmation << "\n";

    MPI_Barrier(MPI_COMM_WORLD);
    cout << "\nbarrier unlocked! - all the clients have received the initial message confirmation\n";

    download_thread_arg download_arg;
    download_arg.rank = rank;
    download_arg.wanted_files = &wanted_files;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &download_arg);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    cout << "num tasks: " << numtasks << "\n";

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
