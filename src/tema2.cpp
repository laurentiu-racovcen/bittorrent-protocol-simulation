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

void *download_thread_func(void *arg) {
    download_thread_arg *download_arg = (download_thread_arg*) arg;

    /* request all wanted files swarms from tracker */
    for (size_t i = 0; i < download_arg->wanted_file_names->size(); i++) {
        cout << "client " << download_arg->rank << ": sending to tracker a file request of file: " << download_arg->wanted_file_names->at(i).c_str() << "\n";

        /* generate request message */
        message_t message;
        message.code = CLIENT_REQUEST_CODE;
        message.source_rank = download_arg->rank;
        strcpy(message.content, download_arg->wanted_file_names->at(i).c_str());

        MPI_Send(&message, sizeof(message_t), MPI_CHAR, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);
        cout << "\nclient " << download_arg->rank << ": sent request message of file " << download_arg->wanted_file_names->at(i) << "\n";

        /* receive file swarm from tracker */
        file_info_t recv_file_info;
        MPI_Status status;
        MPI_Recv(&recv_file_info, sizeof(file_info_t), MPI_BYTE, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD, &status);
        cout << "\nclient " << download_arg->rank
            << ": received from tracker the swarm of file " << recv_file_info.name
            << ", last seg hash =" << recv_file_info.segments_hashes[recv_file_info.segments_num-1]
            << "\n";
    }

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
            current_file_info.swarm.peers_num = 0;
            current_file_info.swarm.seeds_num = 0;
            current_file_info.swarm.seeds[current_file_info.swarm.seeds_num] = i;
            current_file_info.swarm.seeds_num++;

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
                    /* the tracker tracks this file, send the swarm of the requested file to the client */
                    MPI_Send(&files_info[file_index], sizeof(file_info_t), MPI_BYTE, client_message.source_rank, TRACKER_RANK, MPI_COMM_WORLD);
                    cout << "tracker: sent swarm for file " << files_info[file_index].name << " to client " << client_message.source_rank << "!\n";
                } else {
                    cout << "tracker: does not track the file: " << client_message.content << "\n";
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
                    vector<string> *wanted_file_names)
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
        string current_wanted_file;
        getline(fin, current_wanted_file);
        wanted_file_names->push_back(current_wanted_file);
    }
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    /* initialize files data */
    client_files_t client_files;
    vector<string> wanted_file_names;

    get_client_data(rank, &client_files, &wanted_file_names);

    cout << "files struct size = " << sizeof(client_files_t) << "\n";

    for (size_t i = 0; i < client_files.files_num; i++) {
        cout << "file name: " << client_files.files[i].name << "\n";
        cout << "file segments number: " << client_files.files[i].segments_num << "\n";
        cout << "segments:\n";

        /* add current file segments and increment the pointer */
        for (size_t j = 0; j < client_files.files[i].segments_num; j++) {
            cout << client_files.files[i].segments_hashes[j] << "\n";
        }
    }

    cout << "wanted file names:\n";
    for (size_t i = 0; i < wanted_file_names.size(); i++) {
        cout << wanted_file_names[i] << "\n";
    }

    /* send this client's files data (file names and segments) to the tracker */
    // cout << "send buffer size: " << sizeof(client_files_t) << "\n";
    // cout << "sending all the files: " << "\n";
    MPI_Send(&client_files, sizeof(client_files_t), MPI_BYTE, TRACKER_RANK, rank, MPI_COMM_WORLD);
    // cout << "\nclient: message sent! \n";

    /* wait for the confirmation message from tracker */
    MPI_Status recv_status;
    char confirmation[4];
    MPI_Recv(&confirmation, sizeof(client_files_t), MPI_BYTE, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD, &recv_status);

    if (strcmp(confirmation, "ACK") == 0) {
        cout << "client " << rank << " received initial confirmation! confirmation message: " << confirmation << "\n";
    } else {
        exit(-2);
    }

    download_thread_arg download_arg;
    download_arg.rank = rank;
    download_arg.wanted_file_names = &wanted_file_names;

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

