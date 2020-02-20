#include <pthread.h>

#include "rockrdb.h"
#include "rocksdbapi.h"


/* read length of bytes from pipe file descriptor, 
 * return 1 if success read(including len==0), return 0 if pipe close, -1 if error */
int _read_pipe_by_length(int fd, char *buf, size_t len) {
    ssize_t ret;

    while (len) {
        ret = read(fd, buf, len);
        if (ret == 0) 
            return 0;
        else if (ret < 0) 
            return -1;
        else {
            buf += ret;
            len -= ret;
        }
    }
    return 1;
}

/* write length of bytes from pipe file descriptor, 
 * return 1 if success write(including len==0), -1 if error */
int _write_pipe_by_length(int fd, char *buf, size_t len) {

    ssize_t ret;

    while (len) {
        ret = write(fd, buf, len);
        if (ret == 0) 
            serverLog(LL_WARNING, "_write_pipe_by_length() write zero bytes!");
        else if (ret < 0) 
            return -1;
        else {
            buf += ret;
            len -= ret;
        }
    }
    return 1;
}

/* clear all fds in the rdb servcie thread & child rdb process when exit 
 * NOTE: fd may be closed before, but it does not matter */
void _closeRockRdbParamsPipes(RockRdbParams *params) {
    close(params->pipe_request[0]);
    close(params->pipe_request[1]);
    close(params->pipe_response[0]);
    close(params->pipe_response[1]);
}

/* after initRockSerivceForRdbInMainThread(), the rdb service thread has been created
 * the service thread is in sleep state and is waiting for the mutex 
 * when main thread fork the rdb child process, it needs to call this func 
 * to wake up the rdb service thread to service for the rdb child process */
void wakeupRdbServiceThreadAfterForkInMainThread(RockRdbParams *params) {
    if (pthread_mutex_unlock(&params->mutex) != 0) {
        serverLog(LL_WARNING, "wakeupRdbServiceThreadAfterForkInMainThread() failed!");
    }
}

/* service thread for rdb process to request value in the snapshot */
void *_serviceForRdbChildProcessInServiceThread(void *arg) {
    serverLog(LL_NOTICE, "rdb service new born here! rdb service thread id = %d", (int)pthread_self());

    RockRdbParams *params = arg;

    // waiting for main thread unlock the mutex(after fork()) to wake up and start working
    pthread_mutex_lock(&params->mutex);   
    pthread_mutex_unlock(&params->mutex); 

    serverLog(LL_NOTICE, "rdb service wake up! rdb service thread id = %d", (int)pthread_self());

    close(params->pipe_request[1]);     // do not need write-end of request
    close(params->pipe_response[0]);    // do not need read-end of response

    int ret;
    sds key = NULL;
    void *db_val = NULL;

    while(1) {
        // read request
        int dbi;
        ret = _read_pipe_by_length(params->pipe_request[0], (char*)&dbi, sizeof(int));
        if (ret == 0)
            break;  // normal exit because reqeust pipe close
        if (ret == -1) {
            serverLog(LL_WARNING, "rdb servive thread: read request dbi, read error!");
            goto err;
        }

        size_t key_len;
        ret = _read_pipe_by_length(params->pipe_request[0], (char*)&key_len , sizeof(size_t));
        if (ret == 0) {
            serverLog(LL_WARNING, "rdb service thread: read request key len, pipe close!");
            goto err;
        }
        if (ret == -1) {
            serverLog(LL_WARNING, "rdb service thread: read request key len, read error!");
            goto err;
        }

        key = sdsnewlen(NULL, key_len);
        ret = _read_pipe_by_length(params->pipe_request[0], key, key_len);
        if (ret == 0) {
            serverLog(LL_WARNING, "rdb service thread: read request key, pipe close!");
            goto err;
        }
        if (ret == -1) {
            serverLog(LL_WARNING, "rdb service thread: read request key, read error!");
            goto err;
        } 

        // read rocksdb snapshot
        size_t val_len;
        rocksdbapi_read_from_snapshot(dbi, key, key_len, &db_val, &val_len);
        if (db_val == NULL) {
            // not found
            serverLog(LL_WARNING, "rdb service thread: call rocksdbapi(snapshot), return NOT_FOUND!");
            goto err;
        }

        // response
        ret = _write_pipe_by_length(params->pipe_response[1], (char*)&val_len, sizeof(size_t));
        if (ret == -1) {
            serverLog(LL_WARNING, "rdb service thread: write response val len, write error!");
            goto err;
        }
        ret = _write_pipe_by_length(params->pipe_response[1], (char*)db_val, val_len);
        if (ret == -1) {
            serverLog(LL_WARNING, "rdb service thread: write response val, write error!");
            goto err;
        }

        sdsfree(key);
        zfree(db_val);
        key = NULL;
        db_val = NULL;
        // waiting for next request until pipe close
    }

    serverAssert(key == NULL && db_val == NULL);
    _closeRockRdbParamsPipes(params);
    serverLog(LL_NOTICE, "rdb service exit successfully! rdb service thread id = %d", (int)pthread_self());
    zfree(arg);
    rocksdbapi_releaseAllSnapshots();
    return NULL;    // thread exit with success

err:
    if (key) sdsfree(key);
    if (db_val) zfree(db_val);
    _closeRockRdbParamsPipes(params);
    serverLog(LL_NOTICE, "rdb service exit with error! rdb service thread id = %d", (int)pthread_self());
    zfree(arg);
    rocksdbapi_releaseAllSnapshots();
    return NULL;    // thread exit with error
}

/* when a child rdb process start, call this func for initialization */
void initForRockInRdbProcess(RockRdbParams *params) {
    close(params->pipe_request[0]);     // do not need read-end for request pipe
    close(params->pipe_response[1]);    // do not need write-end for response pipe
}

/* when the child rdb process exit, call this func for clear resource */
void clearForRockWhenExitInRdbProcess(RockRdbParams *params) {
    // it will close the request pipe, signal the service thread to exit
    _closeRockRdbParamsPipes(params);    
}

/* the func is called by a rdb child process, the caller need to clear the returned val
 * if something error, return NULL */
sds requestSnapshotValByKeyInRdbProcess(int dbi, sds key, RockRdbParams *params) {
    int ret;

    // first send the key(dbi) to the request pipe
    ret = _write_pipe_by_length(params->pipe_request[1], (void*)&dbi, sizeof(int));
    if (ret == -1) return NULL;
    
    size_t key_len = sdslen(key);
    ret = _write_pipe_by_length(params->pipe_request[1], (void*)&key_len, sizeof(size_t));
    if (ret == -1) return NULL;

    ret = _write_pipe_by_length(params->pipe_request[1], (void*)key, key_len);
    if (ret == -1) return NULL;

    // then read the response from response pipe
    size_t val_len;
    ret = _read_pipe_by_length(params->pipe_response[0], (void*)&val_len, sizeof(size_t));
    if (ret <= 0) return NULL;

    sds val = sdsnewlen(NULL, val_len);
    ret = _read_pipe_by_length(params->pipe_response[0], (void*)val, val_len);
    if (ret <= 0) {
        sdsfree(val);
        return NULL;
    }
    serverAssert(val != NULL);
    return val;
}

/* create a service thread for rdb process's requests for snapshot read in rocksdb 
 * return 1 if success. -1 if error */
int initRockSerivceForRdbInMainThread(RockRdbParams *params) {
    pthread_t service_for_rdb;

    if (pipe(params->pipe_request) == -1 || pipe(params->pipe_response) == -1) {
        serverLog(LL_WARNING, "creating pipes for rdb failed!");
        return -1;
    }

    if (pthread_mutex_init(&params->mutex, NULL) != 0) {
        serverLog(LL_WARNING, "creating service thread for rdb failed, init mutex!");
        _closeRockRdbParamsPipes(params);
        return -1;
    }

    // lock the mutex to let the following newborn rdb service thread sleep
    if (pthread_mutex_lock(&params->mutex) != 0) {
        serverLog(LL_WARNING, "main thread lock the mutex failed!");
        return -1;
    }  

    if (pthread_create(&service_for_rdb, NULL, 
        _serviceForRdbChildProcessInServiceThread, params) != 0) {
        pthread_mutex_destroy(&params->mutex);
        _closeRockRdbParamsPipes(params);
        return -1;
    }

    rocksdbapi_createSnapshots();
    
    return 1;    
}

void _test_rdb_service() {
    serverLog(LL_NOTICE, "redis main process id = %d", (int)getpid());

    char test_key[] = "abc";
    char test_val[] = "This is a test for key abc, really???...";
    rocksdbapi_write(0, test_key, sizeof(test_key)-1, test_val, sizeof(test_val)-1);
    serverLog(LL_NOTICE, "main thread init write key");

    RockRdbParams *params = zmalloc(sizeof(RockRdbParams));

    int ret;

    ret = initRockSerivceForRdbInMainThread(params);

    if (ret != 1) {
        serverLog(LL_NOTICE, "fail init rdb srvice thread");
        zfree(params);
        return;
    } 
    
    serverLog(LL_NOTICE, "success init rdb srvice thread, main thread id = %d", (int)pthread_self());

    ret = fork();
    if (ret == -1) {
        serverLog(LL_NOTICE, "fork failed!");
        zfree(params);
        return ;
    }

    if (ret == 0) {
        // child process
        serverLog(LL_NOTICE, "rdb child process start, pid = %d", (int)getpid());
        initForRockInRdbProcess(params);
        serverLog(LL_NOTICE, "rdb child process init, pid = %d", (int)getpid());
        sds key = sdsnew("abc");
        serverLog(LL_NOTICE, "start to find key %s in child process, pid = %d", key, (int)getpid());
        sds val = requestSnapshotValByKeyInRdbProcess(0, key, params);
        if (val == NULL) 
            serverLog(LL_NOTICE, "get NULL val in child rdb proces, pid = %d", (int)getpid());
        else
            serverLog(LL_NOTICE, "get val in child rdb proces, pid = %d, val = %s", (int)getpid(), val);        
        serverLog(LL_NOTICE, "rdb child process clear before exit, pid = %d", (int)getpid());
        clearForRockWhenExitInRdbProcess(params);
        serverLog(LL_NOTICE, "rdb child process clear exit, pid = %d", (int)getpid());
        _exit(0);
    } else {
        // parent process
        wakeupRdbServiceThreadAfterForkInMainThread(params);
        // params will be freed in rdb service thread when exit
    }
}
