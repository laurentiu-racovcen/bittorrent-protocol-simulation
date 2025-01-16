# **BitTorrent Protocol Simulation**

>The goal of this project is to simulate the BitTorrent peer-to-peer file sharing protocol using MPI. This protocol allows users to share files on the Internet in a decentralized manner. Downloading files using BitTorrent is considered to be faster than HTTP or FTP due to the lack of a central server, which could limit the bandwidth.

## **Table of contents**

1. [Structures](#structures)
2. ["main" function](#main-function)
3. [Tracker function](#tracker-function)
4. [Peer function](#peer-function)
    - [Download thread function](#download-thread-function)
    - [Upload thread function](#upload-thread-function)
5. [Additional functions](#additional-functions)

## **Structures**
The `"utils.h"` header file contains the following structures:
- `"client_file_t"` - stores the information of a client's file: the name of the file, the segments hashes and the segments hashes number.
- `"client_files_t"` - stores the information of all client's files of type `client_file_t`. It is used by the clients and for the transmission of all the files information of a client to the tracker.
- `"swarm_info_t"` - stores the information of a file's swarm: the peers list and the seeds list.
- `"file_info_t"` - stores the information of a file: the file name, the swarm, the owner rank and the segments hashes. It is used by the tracker and for file information exchange between the tracker and the clients.
- `"wanted_file_info_t"` - it is used by clients to store the information of a wanted file: the file name, the swarm, the segments hashes and the received segments.
- `"download_thread_arg"` - it is used by the client's download thread to extract a client's rank and list of wanted files from the `"download_thread_func"` function argument.
- `"upload_thread_arg"` - it is used by the client's upload thread to extract a client's rank and list of files from the `"upload_thread_func"` function argument.

## **"main" function**

**1.** Process program's arguments.

**2.** Initialize tracker and peers' processes.

**3.** Finalize tracker and peers' processes.

## **Tracker function**

**1.** Receives all clients' shared files information and stores it in the `"files_info"` vector. After receiving the information of a client, it sends an `"ACK"` message back to the client.

**2.** Processes all the incoming messages, depending on the message code:
- `"CLIENT_REQUEST_CODE"` - means that the message is a client request of a file's information (segment hashes and swarm). If the tracker has the requested file in its `"files_info"` vector, it sends the file's information and marks the source client as peer of the requested file.
- `"FINISHED_DOWNLOAD_FILE_CODE"` - means that the message's source client finished downloading a certain file (the file name is located in the `"content"` field of the message). The tracker removes the client from the file's peers list and adds it to the file's seeds list.
- `"FINISHED_ALL_DOWNLOADS_CODE"` - means that the message's source client finished downloading all of its wanted files. When receiving this code,
it inserts the source client rank into the `finished_clients` set. If the `finished_clients`'s size is the same as the number of clients, the tracker sends to all the clients a message with the `ALL_CLIENTS_FINISHED_DOWNLOADING_CODE` code.

## **Peer function**

**1.** Reads client's files information and client's wanted files names from the client's input file, and stores them in the `"client_files"` structure and respectively `"wanted_files"` vector.

**2.** Sends all client's files information to the tracker and waits for an `"ACK"` message from the tracker.

**3.** After receiving the `"ACK"` message from the tracker, the client creates 2 threads: a `download thread` and an `upload thread`.

**4.** Calls `join` function for both created threads.

### **Download thread function**:
1. Calls the [`"get_swarms_from_tracker"`](#get_swarms_from_tracker) function in order to receive from the tracker the swarm list and segments hashes of all the wanted files.
2. In the while loop, for every wanted file, sends one segment request at a time, until the client receives all the segments of all the wanted files. After 10 downloaded segments, the client sends files information requests to the tracker in order to update all the wanted files' swarms. For efficiency purposes, the variable `"requests_counter"` counts the number of requests that are sent to the file's swarm clients. The counter is used to keep track of the client type for which every segment request is made: the first 6 segments requests are sent to random-chosen peers from the peers list and the last 4 segments requests are sent to random-chosen seeds from the seeds list. In this way, the peers prioritization reduces the chances of seeds becoming overloaded (especially at the beginning of the files download), and helps balancing the network load.
3. If all the segments of a file have been downloaded, the file is marked as received and the corresponding output file is written.
4. If all the wanted files have been downloaded, sends a `"FINISHED_ALL_DOWNLOADS_CODE"` message to the tracker and then exits.

### **Upload thread function**:
In the while loop, processes every received message, depending on the message code:
- `"SEGMENT_REQUEST_CODE"` - calls the function [`"get_file_segment"`](#get_file_segment) (that extracts the requested segment from the client files) and sends the requested file segment to the source client.
- `"ALL_CLIENTS_FINISHED_DOWNLOADING_CODE"` - the thread exits.

## **Additional functions**

### **Functions used by the Peer:**
#### `get_swarms_from_tracker`:
- Function that requests from tracker all the wanted files swarms and stores them.

#### `get_file_segment`:
- Function that generates an "ACK" or "NACK" response message for a requested segment:
    -   "ACK" - if the requested file segment exists in the client's files.
    -   "NACK" - if the requested file segment doesn't exist in the client's files.

#### `send_next_segment_request`:
- Function that sends a non-received segment request message to a random peer/seed, and then receives the requested segment.

### **Functions used by the Tracker:**
#### `get_tracked_file_index`:
- Function that returns the index of a searched file or `-1` if the file is not in the tracker's file list.




<hr>



**(c) Racovcen Laurențiu**
