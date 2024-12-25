#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string.h>

#include "utils.h"

using namespace std;

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

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
    /* TODO: initialize the file-seeds list:
     * map<file_name, seeds list>
     */

    /* mutex pentru cerere lista seeds si pentru trimitere mesaje */


    /* cand a primit mesajele de la toti clientii,
     * trimite un "ACK" catre toti clientii */
}

/* Function that reads a file and stores its data */
void get_client_data(int rank,
                    vector<client_file_t> *client_files,
                    vector<string> *wanted_file_names,
                    unsigned long *initial_buffer_size)
{
    /* read peer's file */
    ifstream fin("in" + to_string(rank) + ".txt");

    /* read peer's files number */
    string line;
    getline(fin, line);
    unsigned long files_num = stoul(line);

    /* initialize files data */
    cout << "peer files nr = " + to_string(files_num) + ", peer rank = " + to_string(rank) + "\n";

    /* initialize initial buffer size */
    (*initial_buffer_size) = sizeof(unsigned long);

    cout << "init buf siz = " << (*initial_buffer_size);

    /* read peer's files details */
    for (size_t i = 0; i < files_num; i++) {
        /* read current file's details */
        string file_details_line;
        getline(fin, file_details_line);
        stringstream file_details_stream(file_details_line);

        /* extract current file name */
        client_file_t current_file;
        file_details_stream >> current_file.name;

        /* add current file name size in buffer size */
        (*initial_buffer_size) += sizeof(unsigned long);
        (*initial_buffer_size) += current_file.name.size();

        /* extract current file segments number */
        string segments_num_string;
        file_details_stream >> segments_num_string;
        current_file.segments_num = stoul(segments_num_string);

        /* add current file segments number in buffer size */
        (*initial_buffer_size) += sizeof(unsigned long);

        /* add current file segments size in buffer size */
        (*initial_buffer_size) += HASH_SIZE * current_file.segments_num;

        /* extract current file segments */
        for (size_t j = 0; j < current_file.segments_num; j++) {
            string current_segment;
            getline(fin, current_segment);
            current_file.segments.push_back(current_segment);
        }

        /* insert current file details in vector */
        client_files->push_back(current_file);
        cout << "current buf size = " << (*initial_buffer_size);
    }

    /* read peer's wanted files */
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
    vector<client_file_t> files;
    vector<string> wanted_file_names;

    /* declare initial buffer size */
    unsigned long initial_buffer_size = 0;
    get_client_data(rank, &files, &wanted_file_names, &initial_buffer_size);

    /* initialize initial send buffer */
    char *initial_send_buffer = (char*) calloc(initial_buffer_size, sizeof(char));
    char *buf_ptr_copy = initial_send_buffer;

    /* copy the files number and increment the pointer */
    unsigned long files_num = files.size();
    memcpy(initial_send_buffer, &files_num, sizeof(unsigned long));
    initial_send_buffer += sizeof(unsigned long);

    for (size_t i = 0; i < files.size(); i++) {
        /* add current file name and increment the pointer
         * (not NULL-terminated) */
        unsigned long file_name_size = files[i].name.size();
        memcpy(initial_send_buffer, &file_name_size, sizeof(unsigned long));
        initial_send_buffer += sizeof(unsigned long);
        memcpy(initial_send_buffer, ((char*) files[i].name.c_str()), files[i].name.size());
        initial_send_buffer += files[i].name.size();

        cout << "file name: " << files[i].name << "\n";
        cout << "file segments number: " << files[i].segments_num<< "\n";

        /* add current file segments number and increment the pointer */
        memcpy(initial_send_buffer, &(files[i].segments_num), sizeof(unsigned long));
        initial_send_buffer += sizeof(unsigned long);

        /* add current file segments and increment the pointer */
        for (size_t j = 0; j < files[i].segments_num; j++) {
            memcpy(initial_send_buffer, files[i].segments[j].c_str(), HASH_SIZE);
            initial_send_buffer += HASH_SIZE;
            cout << files[i].segments[j] << "\n";
        }
    }

    cout << "segments:\n";
    cout << "wanted file names:\n";
    for (size_t i = 0; i < wanted_file_names.size(); i++) {
        cout << wanted_file_names[i] << "\n";
    }

    /* copy the initial pointer of the buffer */
    initial_send_buffer = buf_ptr_copy;

    /* send this client's files data (file names and segments) to the tracker */
    cout << "send buffer size: " << initial_buffer_size << "\n";
    cout << "sending all the files: " << "\n";
    MPI_Send(initial_send_buffer, initial_buffer_size, MPI_CHAR, TRACKER_RANK, rank, MPI_COMM_WORLD);

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

