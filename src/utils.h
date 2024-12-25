#ifndef _UTILS_H_
#define _UTILS_H_

#include <iostream>

using namespace std;

typedef struct {
    string name;
    unsigned long segments_num;
    vector<string> segments;
} client_file_t;

#endif
