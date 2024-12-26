#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string.h>

#include "utils.h"

using namespace std;

void *download_thread_func(void *arg) {
    int rank = *(int*) arg;

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
    cout << "tracker rank: "<< rank << "\n";

    for (int i = 1; i < numtasks; i++) {
        MPI_Status status;
        client_files_t client_files;
        cout << "\ntracker: receiving message...\n";

        MPI_Recv(&client_files, sizeof(client_files_t), MPI_BYTE, i, i, MPI_COMM_WORLD, &status);
        cout << "\ntracker: message received from client " << i
            << "\n first file name is: " << client_files.files[0].name
            << "\n first file num seg: " << client_files.files[0].segments_num
            << "\n files number: " << client_files.files_num
            << "\n first seg: " << client_files.files[0].segments[0] 
            << "\n last seg: " << client_files.files[client_files.files_num-1].segments[client_files.files[client_files.files_num-1].segments_num-1]
            << "\n tracker: status received: " << status._ucount << "\n";
    }

    /* TODO: initialize the file-seeds list:
     * map<file_name, seeds list>
     */

    /* mutex pentru cerere lista seeds si pentru trimitere mesaje */


    /* cand a primit mesajele de la toti clientii,
     * trimite un "ACK" catre toti clientii */
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
            string current_segment;
            getline(fin, current_segment);
            strcpy(client_files->files[i].segments[j], current_segment.c_str());
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

    /* declare initial buffer size */
    get_client_data(rank, &client_files, &wanted_file_names);

    cout << "files struct size = " << sizeof(client_files_t) << "\n";

    for (size_t i = 0; i < client_files.files_num; i++) {
        cout << "file name: " << client_files.files[i].name << "\n";
        cout << "file segments number: " << client_files.files[i].segments_num << "\n";
        cout << "segments:\n";

        /* add current file segments and increment the pointer */
        for (size_t j = 0; j < client_files.files[i].segments_num; j++) {
            cout << client_files.files[i].segments[j] << "\n";
        }
    }

    cout << "wanted file names:\n";
    for (size_t i = 0; i < wanted_file_names.size(); i++) {
        cout << wanted_file_names[i] << "\n";
    }

    /* send this client's files data (file names and segments) to the tracker */
    cout << "send buffer size: " << sizeof(client_files_t) << "\n";
    cout << "sending all the files: " << "\n";
    MPI_Send(&client_files, sizeof(client_files_t), MPI_BYTE, TRACKER_RANK, rank, MPI_COMM_WORLD);
    cout << "\nclient: message sent! \n";

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
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

