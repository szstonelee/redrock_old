#ifndef __ROCKRDB_H
#define __ROCKRDB_H

#include "server.h"

typedef struct RockRdbParams {
    int pipe_request[2];
    int pipe_response[2];
    pthread_mutex_t mutex;
} RockRdbParams;  


#endif

