#ifndef __ROCKRDB_H
#define __ROCKRDB_H

#include "server.h"

typedef struct RockRdbParams {
    int pipe_request[2];
    int pipe_response[2];
    pthread_mutex_t mutex;
    struct RockRdbParams **myself;
} RockRdbParams;  

void _test_rdb_service();
void clearForRockWhenExitInRdbProcess(RockRdbParams *params);
int initRockSerivceForRdbInMainThread(RockRdbParams *params);
void initForRockInRdbProcess(RockRdbParams *params);
void wakeupRdbServiceThreadAfterForkInMainThread(RockRdbParams *params);
sds requestSnapshotValByKeyInRdbProcess(int dbi, sds key, RockRdbParams *params);

#endif

