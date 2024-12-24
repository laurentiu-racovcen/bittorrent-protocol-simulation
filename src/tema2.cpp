#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

void *download_thread_func(void *arg) {
    int rank = *(int*) arg;

    return NULL;
}

void *upload_thread_func(void *arg) {
    int rank = *(int*) arg;

    return NULL;
}

void tracker(int numtasks, int rank) {

}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    /* read peer's file */
    ifstream fin("in" + to_string(rank) + ".txt");

    /* read peer's files number */
    string line;
    getline(fin, line);
    unsigned long files_num = stoul(line);

    cout << "peer files nr = " + to_string(files_num) + ", peer rank = " + to_string(rank) + "\n";

    /* read peer's files segments */
    for (size_t i = 0; i < files_num; i++) {
        /* read current file's details */
        string file_details_line;
        getline(fin, file_details_line);
        stringstream file_details_stream(file_details_line);

        /* extract current file name */
        string file_name;
        file_details_stream >> file_name;

        /* extract current file segments number */
        string segments_num_string;
        unsigned long segments_num;
        file_details_stream >> segments_num_string;
        segments_num = stoul(segments_num_string);

        cout << "file name: " << file_name << "\n";
        cout << "file segments number: " << segments_num << "\n";
        cout << "segments:\n";

        for (size_t i = 0; i < segments_num; i++) {
            string current_segment;
            getline(fin, current_segment);
            cout << current_segment << "\n";
        }
    }

    /* read peer's wanted files */
    getline(fin, line);
    unsigned long wanted_files_num = stoul(line);\

    cout << "wanted file names:\n";

    /* read peer's wanted files' names */
    for (size_t i = 0; i < wanted_files_num; i++) {
        /* read current file's name */
        string current_file_name;
        getline(fin, current_file_name);
        cout << current_file_name << "\n";
    }

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

