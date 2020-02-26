/*
 * Copyright (c) 2020-, szstonelee <szstonelee at vip qq com>
 * All rights reserved.
 * 
 * mainly based on Redis & Rocksdb, please check their rights
 * redis: https://github.com/antirez/redis/blob/unstable/COPYING
 * rocksdb: https://github.com/facebook/rocksdb/blob/master/COPYING
 * and other 3rd libaries:
 * snappy: https://github.com/google/snappy/blob/master/COPYING
 * lz4: https://github.com/lz4/lz4/blob/dev/LICENSE
 * bzip2: https://sourceware.org/git/bzip2.git
 * zstd: https://github.com/facebook/zstd/blob/dev/COPYING
 * zlib: http://zlib.net/ 
 * jemalloc: http://jemalloc.net/
 * lua: http://www.lua.org/license.html
 * hiredis: https://github.com/redis/hiredis
 * linenoise: check Readme.markdown in deps/linenoise 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "rock.h"
#include "rockserdes.h"
#include "rocksdbapi.h"

#if defined(__APPLE__)
    #include <os/lock.h>
    static os_unfair_lock spinLock;
    // static os_unfair_lock spinLock = OS_UNFAIR_LOCK_INIT;
    #define rocklock() os_unfair_lock_lock(&spinLock)
    #define rockunlock() os_unfair_lock_unlock(&spinLock)

    void initSpinLock() {
        spinLock = OS_UNFAIR_LOCK_INIT;
    }
#else
    #include <pthread.h>
    static pthread_spinlock_t spinLock;
    void initSpinLock() {
        pthread_spin_init(&spinLock, 0);
    }    
    #define rocklock() pthread_spin_lock(&spinLock)
    #define rockunlock() pthread_spin_unlock(&spinLock)
#endif

#define ROCK_THREAD_MAX_SLEEP_IN_US 1024

/* safe space assume to be 32M before eviction */
#define SAFE_MEMORY_ROCK_BEFORE_EVIC    (32<<20) 
#define MAX_TRY_PICK_KEY_TIMES 64

/* check whether the config is enabled 
 * NOTE: we set these config parameter is mutable, but actually it can not be changed online 
 * return 1 if enabled, otherwise 0*/
int isRockFeatureEnabled() {
    if (server.enable_rocksdb_feature && server.maxmemory > 0) 
        return 1;
    else
        return 0;
}


/* please reference EVPOOL_SIZE, EVPOOL_CACHED_SDS_SIZE */
#define RKPOOL_SIZE 16
#define RKPOOL_CACHED_SDS_SIZE 255
struct rockPoolEntry {
    unsigned long long idle;    /* Object idle time (inverse frequency for LFU) */
    sds key;                    /* Key name. */
    sds cached;                 /* Cached SDS object for key name. */
    int dbid;                   /* Key DB number. */
};

/* init every hotKeys from every db's dict */
void initHotKeys() {
    dictEntry *de;
    for (int i = 0; i < server.dbnum; ++i) {
        serverAssert(dictSize(server.db[i].hotKeys) == 0);
        dictIterator *dit = dictGetIterator(server.db[i].dict);
        while ((de = dictNext(dit))) {
            if (dictGetVal(de) != shared.valueInRock) 
                dictAdd(server.db[i].hotKeys, dictGetKey(de), NULL);
            else
                serverLog(LL_NOTICE, "initHotKeys ERROR!!! found a rock key, key = %s", dictGetKey(de));            
        }
        /*
        if (dictSize(server.db[i].dict)) {            
            serverLog(LL_NOTICE, "all key size = %lu, hot key = %lu", 
                dictSize(server.db[i].dict), dictSize(server.db[i].hotKeys));
        }
        */        
        dictReleaseIterator(dit); 
    }
}

static struct rockPoolEntry *RockPoolLRU;

void rock_test_read_rockdb(char *key) {
    void *val;
    size_t len;
    rocksdbapi_read(0, key, strlen(key), &val, &len);
    if (val == NULL) {
        serverLog(LL_NOTICE, "db read is null!");
    } else {
        if (len == 0)
            serverLog(LL_NOTICE, "db read key = %s, val len = %lu, val empty string", key, len);
        else
            serverLog(LL_NOTICE, "db read key = %s, val len = %lu, val = %s", key, len, val);
    }
}

void rock_test_write_rockdb(char *val) {
    char *key = "abcd";

    rocksdbapi_write(0, key, strlen(key), val, strlen(val));
}

/* error return -1, 1 init ok */
int init_rocksdb(int dbnum, char *path) {
    // if (!server.enable_rocksdb_feature) return 0;
    serverAssert(isRockFeatureEnabled());

    if (!server.maxmemory) {
        serverLog(LL_NOTICE, "enable rocksdb but maxmemory is zero! error config!");
        return -1;
    }

    if (!path || strlen(path) == 0) {
        serverLog(LL_NOTICE, "rockdb path is empty!");
        return -1;
    }

    if (path[strlen(path)-1] != '/') {
        serverLog(LL_NOTICE, "config rockdbdir not correct! last char need to be '/' !!!!!!");
        return -1;
    }

    rocksdbapi_init(dbnum, path);
    return 1;
}

void teardown_rocksdb() {
    rocksdbapi_teardown();
}

/* reference evicPoolAlloc() */
void rockPoolAlloc(void) {
    struct rockPoolEntry *rp;
    int j;

    rp = zmalloc(sizeof(*rp)*RKPOOL_SIZE);
    for (j = 0; j < RKPOOL_SIZE; j++) {
        rp[j].idle = 0;
        rp[j].key = NULL;
        rp[j].cached = sdsnewlen(NULL,RKPOOL_CACHED_SDS_SIZE);
        rp[j].dbid = 0;
    }
    RockPoolLRU = rp;
}

void incrRockCount() {
    long old = (long)shared.valueInRock->ptr;
    shared.valueInRock->ptr = (void*)(++old);
}

int getRockKeyCountInDb(int dbid) {
    int count = 0;
    dictIterator *dit = dictGetIterator(server.db[dbid].dict);
    dictEntry *de;
    while ((de = dictNext(dit))) {
        robj *o = dictGetVal(de);
        if (o == shared.valueInRock) ++count;
    }
    dictReleaseIterator(dit);
    return count;
}

/* reference evictPoolPopulate() */
void rockPoolPopulate(int dbid, dict *sampledict, dict *keydict, struct rockPoolEntry *pool) {
    int j, k, count;
    dictEntry *samples[server.maxmemory_samples];

    count = dictGetSomeKeys(sampledict,samples,server.maxmemory_samples);
    for (j = 0; j < count; j++) {
        unsigned long long idle;
        sds key;
        robj *o;
        dictEntry *de;

        de = samples[j];
        key = dictGetKey(de);
        serverAssert(key);
        serverAssert (sampledict != keydict); 
        de = dictFind(keydict, key);
        
        if (de == NULL) {            
            serverLog(LL_NOTICE, "dbid = %d, db size = %lu, hotkey size = %lu, rock key total = %d", 
                dbid, dictSize(server.db[dbid].dict), dictSize(server.db[dbid].hotKeys), getRockKeyCountInDb(dbid));             
            serverLog(LL_NOTICE, "key = %s", key);
            serverLog(LL_NOTICE, "j = %d, count = %d", j, count);           
            exit(1);
        }

        o = dictGetVal(de);
        serverAssert(o);

        /* Calculate the idle time according to the policy. This is called
         * idle just because the code initially handled LRU, but is in fact
         * just a score where an higher score means better candidate. 
         * NOTE: different from evicPoolPopulate(), we only consideer LFU, 
         * otherwise always LRU */
        if (server.maxmemory_policy & MAXMEMORY_FLAG_LFU) {
            /* When we use an LRU policy, we sort the keys by idle time
             * so that we expire keys starting from greater idle time.
             * However when the policy is an LFU one, we have a frequency
             * estimation, and we want to evict keys with lower frequency
             * first. So inside the pool we put objects using the inverted
             * frequency subtracting the actual frequency to the maximum
             * frequency of 255. */
            idle = 255-LFUDecrAndReturn(o);
        } else if (server.maxmemory_policy & MAXMEMORY_FLAG_LRU) {
            idle = estimateObjectIdleTime(o);
        } else {
            idle = ULLONG_MAX - (long)o;
        } 

        /* Insert the element inside the pool.
         * First, find the first empty bucket or the first populated
         * bucket that has an idle time smaller than our idle time. */
        k = 0;
        while (k < RKPOOL_SIZE &&
               pool[k].key &&
               pool[k].idle < idle) k++;
        if (k == 0 && pool[RKPOOL_SIZE-1].key != NULL) {
            /* Can't insert if the element is < the worst element we have
             * and there are no empty buckets. */
            continue;
        } else if (k < RKPOOL_SIZE && pool[k].key == NULL) {
            /* Inserting into empty position. No setup needed before insert. */
        } else {
            /* Inserting in the middle. Now k points to the first element
             * greater than the element to insert.  */
            if (pool[RKPOOL_SIZE-1].key == NULL) {
                /* Free space on the right? Insert at k shifting
                 * all the elements from k to end to the right. */

                /* Save SDS before overwriting. */
                sds cached = pool[RKPOOL_SIZE-1].cached;
                memmove(pool+k+1,pool+k,
                    sizeof(pool[0])*(RKPOOL_SIZE-k-1));
                pool[k].cached = cached;
            } else {
                /* No free space on right? Insert at k-1 */
                k--;
                /* Shift all elements on the left of k (included) to the
                 * left, so we discard the element with smaller idle time. */
                sds cached = pool[0].cached; /* Save SDS before overwriting. */
                if (pool[0].key != pool[0].cached) sdsfree(pool[0].key);
                memmove(pool,pool+1,sizeof(pool[0])*k);
                pool[k].cached = cached;
            }
        }

        /* Try to reuse the cached SDS string allocated in the pool entry,
         * because allocating and deallocating this object is costly
         * (according to the profiler, not my fantasy. Remember:
         * premature optimizbla bla bla bla. */
        int klen = sdslen(key);
        if (klen > RKPOOL_CACHED_SDS_SIZE) {
            pool[k].key = sdsdup(key);
        } else {
            memcpy(pool[k].cached,key,klen+1);
            sdssetlen(pool[k].cached,klen);
            pool[k].key = pool[k].cached;
        }
        pool[k].idle = idle;
        pool[k].dbid = dbid;
    }
}

/* when a client is free, it need to clear its node
 * in each db rockKeys list of clients, for the later safe call
 * from the main thread _rockPipeReadHandler() */
void releaseRockKeyWhenFreeClient(client *c) {
    dictIterator *dit;
    dictEntry *de;
    list *clients;

    for (int i = 0; i< server.dbnum; ++i) {
        if (dictSize(server.db[i].rockKeys) == 0) continue;

        dit = dictGetIterator(server.db[i].rockKeys);
        while ((de = dictNext(dit))) {
            clients = dictGetVal(de);
            serverAssert(clients);
            listIter lit;
            listNode *ln;
            listRewind(clients, &lit);
            while ((ln = listNext(&lit))) {
                if (listNodeValue(ln) == c) listDelNode(clients, ln);
            }
        }
        dictReleaseIterator(dit);
    }
}

/* NOTE: checking new job key does need to lock anything 
 * if all db rockKeys is empty, return 0
 * else, return 1 with one random job of dbid & key  */
int _hasMoreRockJob(int *dbid, sds *key) {
    dictIterator *it;
    dictEntry *entry;

    for (int i = 0; i < server.dbnum; ++i) {
        if (dictSize(server.db[i].rockKeys) == 0) continue;

        it = dictGetIterator(server.db[i].rockKeys);
        entry = dictNext(it);
        serverAssert(entry != NULL);
        *key = dictGetKey(entry);
        *dbid = i;
        dictReleaseIterator(it);
        return 1;
    }
    
    return 0;
}

void initZeroRockJob() {
    rocklock();
    server.rockJob.dbid = -1;
    server.rockJob.workKey = NULL;
    server.rockJob.returnKey = NULL;
    server.rockJob.valInRock = NULL;
    rockunlock();
}

int _isZeroRockJobState() {
    int ret;
    rocklock();
    if (server.rockJob.dbid == -1) {
        ret = 1;
        serverAssert(server.rockJob.returnKey == NULL && server.rockJob.workKey == NULL);
    }
    else {
        ret = 0;
        if (server.rockJob.workKey == NULL)
            serverAssert(server.rockJob.returnKey != NULL);
        else
            serverAssert(server.rockJob.returnKey == NULL);           
    }        
    rockunlock();
    return ret;
}

void _createNewJob(int dbid, sds key) {
    rocklock();
    serverAssert(dbid >= 0 && dbid < server.dbnum);
    serverAssert(server.rockJob.workKey == NULL);   
    server.rockJob.dbid = dbid;
    server.rockJob.workKey = key;
    /* Note: before this point, returnKey maybe not be NULL */
    server.rockJob.returnKey = NULL;
    rockunlock();
}

/* if no key to work, key will be NULL */
void _getRockWork(int *dbid, sds *key) {
    rocklock();
    serverAssert(!(server.rockJob.workKey != NULL && server.rockJob.returnKey != NULL));
    if (server.rockJob.returnKey != NULL) {
        serverAssert(server.rockJob.workKey == NULL);
        *key = NULL;
        *dbid = -1;
    } else {
        *key = server.rockJob.workKey;
        *dbid = server.rockJob.dbid;
    }
    rockunlock();
}

/* when finish loading a value from Rockdb, we need to restore it to the original db,
 * and get the client list which is waiting for the rock key and decrease the rockKeyNumber
 * if the rockKeyNumber is descreased to zero, we need to try resume the client
 * NOTE1: the client need to be check again, becuase it could be resumed to go on for the command
 * NOTE2: when val is going to restore to the db, the key may be deleted (or flushed!)
 * or the val of the key has been updated, we need to check for that 
 * but guarentee not dumping again, so we do not need to worry about the wrong restored value
*/
void _clearFinishKey(int dbid, sds key, robj *val) {    
    listIter li;
    listNode *ln;
    list *clients, *zeroClients;

    zeroClients = listCreate();

    /* restore the val in db */
    dictEntry *entry = dictFind(server.db[dbid].dict, key);
    if (entry && dictGetVal(entry) == shared.valueInRock) {
        dictSetVal(server.db[dbid].dict, entry, val);
        /* afer restore the real value, we need add it to hotKey 
         * NOTE: do not use key, use dictGetKey(entry) */
        int ret = dictAdd(server.db[dbid].hotKeys, dictGetKey(entry), NULL);        
        serverAssert(ret == DICT_OK);
    } // else due to deleted, updated or flushed

    /* adjust rockKeyNumber and get zeroClients */
    /* NOTE: clients maybe empty, because client may be disconnected */
    clients = dictFetchValue(server.db[dbid].rockKeys, key);    
    serverAssert(clients != NULL);
    listRewind(clients, &li);
    while((ln = listNext(&li))) {
        client *c = listNodeValue(ln);
        serverAssert(c && c->rockKeyNumber > 0);
        --c->rockKeyNumber;
        if (c->rockKeyNumber == 0) 
            listAddNodeTail(zeroClients, c);
    }
 
    /* delete the entry in rockKeys */
    int ret = dictDelete(server.db[dbid].rockKeys, key);
    serverAssert(ret == DICT_OK);

    /* try resume the zeroClients */
    listRewind(zeroClients, &li);
    while((ln = listNext(&li))) {
        client *c = listNodeValue(ln);
        checkThenResumeRockClient(c);
    }

   listRelease(zeroClients);
}

/* the event handler is executed from main thread, which is signaled by the pipe
 * from the rockdb thread. When it is called by the eventloop, there is 
 * a return result in rockJob */
void _rockPipeReadHandler(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    UNUSED(mask);
    UNUSED(clientData);
    UNUSED(eventLoop);

    int finishDbid;
    sds finishKey;
    robj *val;

    /* deal with return result */
    rocklock();
    serverAssert(server.rockJob.dbid != -1);
    serverAssert(server.rockJob.workKey == NULL);
    serverAssert(server.rockJob.returnKey != NULL);
    serverAssert(server.rockJob.valInRock != NULL);
    finishDbid = server.rockJob.dbid;
    finishKey = server.rockJob.returnKey;
    val = server.rockJob.valInRock;
    rockunlock();

    _clearFinishKey(finishDbid, finishKey, val);

    int moreJobDbid;
    sds moreJobKey;
    if (_hasMoreRockJob(&moreJobDbid, &moreJobKey)) 
        _createNewJob(moreJobDbid, moreJobKey);
    else 
        initZeroRockJob();

    char tmpUseBuf[1];
    read(fd, tmpUseBuf, 1);     /* maybe unblock the rockdb thread by read the pipe */    
}

void _doRockJobInRockThread(int dbid, sds key, robj **val) {
    void *val_db_ptr;
    size_t val_db_len;

    rocksdbapi_read(dbid, key, sdslen(key), &val_db_ptr, &val_db_len);

    if (val_db_ptr == NULL) {
        serverLog(LL_NOTICE, "_doRockJobInRockThread() get null, key = %s", key);
        *val = NULL;
        return;
    }
    
    // robj *o = desString(val_db_ptr, val_db_len);
    robj *o = desObject(val_db_ptr, val_db_len);
    serverAssert(o);

    // we need to free the memory allocated by rocksdbapi
    zfree(val_db_ptr);

    *val = o;   
}

/* for rdb backup  */
robj* loadValFromRockForRdb(int dbid, sds key) {
    // We tempory call the func for rdb test
    // but it is not correct, because rdb is in another pid
    // in futrue, we need client/server model to solve the problem 

    robj *o;

    if (server.inSubChildProcessState) {
        // serverLog(LL_NOTICE, "In subprocess envirenmont");
        sds val = requestSnapshotValByKeyInRdbProcess(dbid, key, server.rockRdbParams);
        if (val) {
            o = desObject(val, sdslen(val));
            sdsfree(val);
        } else {
            o = NULL;
            serverLog(LL_WARNING, "Rocksdb string val is null in child process!");
        }
    } else {    
        // for main process, we get rocksdb val in main thread
        _doRockJobInRockThread(dbid, key, &o);
    }
    
    return o;
}

int _haveRockJobThenDoJobInRockThread() {
    int dbid;
    sds key;

    _getRockWork(&dbid, &key);

    if (key == NULL) {
        return 0;   
    } else {
        serverAssert(dbid >=0 && dbid < server.dbnum);
        robj *valInRock; 
        _doRockJobInRockThread(dbid, key, &valInRock);

        /* after finish a job, we need return the job result and notify the main thread */
        rocklock();
        serverAssert(server.rockJob.workKey == key && server.rockJob.dbid == dbid && server.rockJob.returnKey == NULL);
        server.rockJob.workKey = NULL;
        server.rockJob.returnKey = key;
        server.rockJob.valInRock = valInRock;
        rockunlock();

        /* signal main thread rockPipeReadHandler()*/
        char tmpUseBuf[1] = "a";
        write(server.rock_pipe_write, tmpUseBuf, 1);
        return 1;
    }
}

/* this is the rock thread entrance, working together with the main thread */
void *_mainProcessInRockThread(void *arg) {
    UNUSED(arg);
    int sleepMicro = 1;

    while(1) {
        if (_haveRockJobThenDoJobInRockThread()) {
            sleepMicro = 1;
        } else {
            if (sleepMicro >= ROCK_THREAD_MAX_SLEEP_IN_US) sleepMicro = ROCK_THREAD_MAX_SLEEP_IN_US;
            usleep(sleepMicro);
            sleepMicro <<= 1;
        }
    }

    return NULL;
}


/* the function create a thread, which will read data from Rocksdb other than the main thread 
 * main thread and rock thread will be synchronized by a spinning lock,
 * and signal (wakeup) the rock thread by the pipe, server.rock_pipe */
void initRockPipe() {
    pthread_t rockdb_thread;
    int pipefds[2];

    if (pipe(pipefds) == -1) serverPanic("Can not create pipe for rock.");

    server.rock_pipe_read = pipefds[0];
    server.rock_pipe_write = pipefds[1];

    if (aeCreateFileEvent(server.el, server.rock_pipe_read, 
        AE_READABLE, _rockPipeReadHandler,NULL) == AE_ERR) {
        serverPanic("Unrecoverable error creating server.rock_pipe file event.");
    }

    if (pthread_create(&rockdb_thread, NULL, _mainProcessInRockThread, NULL) != 0) {
        serverPanic("Unable to create a rock thread.");
    }
}

void _checkRockForSingleCmd(client *c, list *l) {
    if (c->cmd->checkRockProc) 
        c->cmd->checkRockProc(c, c->cmd, c->argv, c->argc, l);
}

void _checkRockForMultiCmd(client *c, list *l) {
    for (int j = 0; j < c->mstate.count; ++j) {
        if (c->mstate.commands[j].cmd->checkRockProc) 
            c->mstate.commands[j].cmd->checkRockProc(
                c, c->mstate.commands[j].cmd, 
                c->mstate.commands[j].argv, c->mstate.commands[j].argc, l);
    }
}

/* check the client keys for the command(s) has any value in Rocksdb 
 * side effects: 
 * 1. update the rock key number waiting for rocksdb value 
 * 2. update the db->rockKeys
 * 3. if not job exist, init a job 
 * NOTE: scripts.c can not call this func, because script need execute as a whole */
void checkCallValueInRock(client *c) {
    serverAssert(c);
    serverAssert(c->rockKeyNumber == 0);

    list *valueInRockKeys;  /* list of sds keys */

    valueInRockKeys = listCreate();

    /* 1. check whether there are any key's value in Rocksdb */
    if (c->flags & CLIENT_MULTI) 
        _checkRockForMultiCmd(c, valueInRockKeys);
    else 
        _checkRockForSingleCmd(c, valueInRockKeys);
    c->rockKeyNumber = listLength(valueInRockKeys);

    /* 2. update db->rockKeys */
    if (c->rockKeyNumber) {
        listIter it;
        listNode *ln;
        list *clients;
        dictEntry *de;
        listRewind(valueInRockKeys, &it);
        while ((ln = listNext(&it))) {
            sds key = listNodeValue(ln);
            serverAssert(key);
            sds copyKey = sdsdup(key);
            de = dictFind(c->db->rockKeys, copyKey);
            if (!de) 
                dictAdd(c->db->rockKeys, copyKey, listCreate());
            de = dictFind(c->db->rockKeys, copyKey);
            serverAssert(de);
            clients = dictGetVal(de);
            serverAssert(clients);    
            listAddNodeTail(clients, c);
            if (dictGetKey(de) != copyKey) sdsfree(copyKey);
        }
    }

    /* 3. check whether need to init a new job */
    if (c->rockKeyNumber && _isZeroRockJobState()) {
        int job_dbid;
        sds job_key;
        int ret = _hasMoreRockJob(&job_dbid, &job_key);
        serverAssert(ret > 0);
        _createNewJob(job_dbid, job_key);
    }

    listRelease(valueInRockKeys);    
}

/* caller guarentee the client not wait for rock keys
 * usually called by checkThenResumeRockClient() */
void _resumeRockClient(client *c) {
    server.current_client = c;

    call(c, CMD_CALL_FULL); 

    c->woff = server.master_repl_offset;
    if (listLength(server.ready_keys))
        handleClientsBlockedOnKeys();

    if (c->flags & CLIENT_MASTER && !(c->flags & CLIENT_MULTI)) {
        /* Update the applied replication offset of our master. */
            c->reploff = c->read_reploff - sdslen(c->querybuf) + c->qb_pos;
    }

    serverAssert(!(c->flags & CLIENT_BLOCKED) || c->btype != BLOCKED_MODULE);

    resetClient(c);

    processInputBuffer(c);
}

/* check that the command(s)(s for transaction) have any keys in rockdb 
 * if no keys in rocksdb, then resume the command
 * otherwise, client go on sleep for rocks */
void checkThenResumeRockClient(client *c) {
    serverAssert(c->rockKeyNumber == 0);
    checkCallValueInRock(c);
    if (c->rockKeyNumber == 0) 
        _resumeRockClient(c);
}

void _debugPrintClient(char *callInfo, client *c) {
    char *clientName = !c->name ? "client_name_null" : c->name->ptr;
    if (strcmp(clientName, "debug") != 0) return;

    uint64_t flags = c->flags;
    size_t qb_pos = c->qb_pos; 
    size_t querybufLen = sdslen(c->querybuf);
    int argc = c->argc;
    char *cmdStr = "cmdStr_null";
    char *firstParam = "first_param_null";
    if (argc) {
        cmdStr = c->argv[0]->ptr;
        if (argc > 1)
            firstParam = c->argv[1]->ptr;
    }
    serverLog(LL_NOTICE, "callInfo = %s: flags(hex) = %llx, qb_pos = %zu, querybufLen = %zu, argc = %d, cmdStr = %s, firstParam = %s", 
        callInfo, flags, qb_pos, querybufLen, argc, cmdStr, firstParam);
}

void rock_print_debug() {
    listIter *it = listGetIterator(server.clients, AL_START_HEAD);

    for (listNode* node = listNext(it); node != NULL; node = listNext(it)) 
        _debugPrintClient("rock_print_debug()", node->value);

    listReleaseIterator(it);
}

void rock_debug_print_key_report() {
    dictEntry *de;
    dictIterator *di;

    long long all_total = 0;
    for (int i = 0; i < server.dbnum; ++i) {
        long total = 0, rock = 0;
        di = dictGetIterator(server.db[i].dict);
        int print_one_key = 0;
        while ((de = dictNext(di))) {
            ++total;
            robj *o = dictGetVal(de);
            if (o == shared.valueInRock) {
                ++rock;
                if (!print_one_key) {
                    sds key = dictGetKey(de);
                    print_one_key = 1;
                    serverLog(LL_NOTICE, "db=%d, one rock key = %s", i, key);
                }
                sds key = dictGetKey(de);
                dictEntry *check = dictFind(server.db[i].hotKeys, key);
                if (check != NULL) 
                    serverLog(LL_NOTICE, "something wrong, rock key in hotKeys! key = %s", key);
            } 
        }

        dictReleaseIterator(di);
        all_total += total;
        if (total)
            serverLog(LL_NOTICE, "db=%d, key total = %ld, value in rock = %ld, hot key = %lu", 
                i, total, rock, dictSize(server.db[i].hotKeys));
    }
    serverLog(LL_NOTICE, "all db key total = %lld", all_total);
}

void rock_test_resume_rock() {
    listIter *it = listGetIterator(server.clients, AL_START_HEAD);

    for (listNode *node = listNext(it); node != NULL; node = listNext(it)) {
        client *c = node->value;
        if (c->rockKeyNumber) {
            c->rockKeyNumber = 0;
            checkThenResumeRockClient(c);
            break;
        }
    }

    listReleaseIterator(it);
}

void rock_test_set_rock_key(char* keyStr) {
    serverLog(LL_NOTICE, "_test_set_rock_key, key = %s", keyStr);
    robj *key = createStringObject(keyStr, strlen(keyStr));
    genericSetKey(server.db, key, shared.valueInRock, 0);
}

void test_add_work_key(int dbid, char *key, size_t len) {
    rocklock();
    serverAssert(server.rockJob.workKey == NULL && server.rockJob.returnKey == NULL && server.rockJob.dbid == -1);
    server.rockJob.dbid = dbid;
    server.rockJob.workKey = sdsnewlen(key, len);
    rockunlock();
}

size_t getMemoryOfRock() {
    if (!isRockFeatureEnabled()) return 0;

    return rocksdbapi_memory();
}


/* estimate the memory usage considering the rockdb engine
 * return C_OK for no need to free memory by dumping to rocksdb, 
 * otherwise return C_ERR, so we need first dump memory to rocksdb first, tofree has th value
*/
int getMmemoryStateWithRock(size_t *tofree) {
    if (!server.maxmemory) return C_OK;     // no need limit in server config, return ASAP

    size_t mem_reported, mem_aof_slave, mem_rock, mem_used;

    mem_reported = zmalloc_used_memory();
 
    // mem_aof_slave is the memory used by slave buffer and AOF buffer 
    mem_aof_slave = freeMemoryGetNotCountedMemory();  
    mem_rock = getMemoryOfRock();

    mem_used = (mem_reported > mem_aof_slave + mem_rock) ? mem_reported - mem_aof_slave - mem_rock : 0;

    // before go to evic, we need keep SAFE_MEMORY_ROCK_BEFORE_EVIC room for rock dump first
    if (mem_used + SAFE_MEMORY_ROCK_BEFORE_EVIC <= server.maxmemory) return C_OK;

    *tofree = mem_used + SAFE_MEMORY_ROCK_BEFORE_EVIC - server.maxmemory;

    serverLog(LL_DEBUG, "maxmemory = %llu, repored =%lu, mem_aof_slave = %lu, rock = %lu, used = %lu, tofree = %lu, safe = %lu", 
        server.maxmemory, mem_reported, mem_aof_slave, mem_rock, mem_used, *tofree, (long)SAFE_MEMORY_ROCK_BEFORE_EVIC);

    return C_ERR;   // we need to try to dump some values to rocksdb
}

void _dumpValToRock(sds key, int dbid) {
    dictEntry *de = dictFind(server.db[dbid].dict, key);
    serverAssert(de);
    robj *val = dictGetVal(de);
    serverAssert(val);
    serverAssert(val != shared.valueInRock && val->refcount == 1);

    sds serVal = serObject(val);
    rocksdbapi_write(dbid, key, sdslen(key), serVal, sdslen(serVal));
    sdsfree(serVal);

    dictSetVal(server.db[dbid].dict, de, shared.valueInRock);    
    decrRefCount(val);

    /* we need delete it from hotKeys */
    int ret = dictDelete(server.db[dbid].hotKeys, key);
    if (ret != DICT_OK) {
        serverLog(LL_NOTICE, "key = %s", key);
        serverLog(LL_NOTICE, "total key = %lu", dictSize(server.db[dbid].dict));
        serverLog(LL_NOTICE, "hotkeys total = %lu", dictSize(server.db[dbid].hotKeys));
        int total_rock_count = 0;
        dictIterator *dit = dictGetIterator(server.db[dbid].dict);
        dictEntry *de;
        while ((de = dictNext(dit))) {
            if (dictGetVal(de) == shared.valueInRock) ++total_rock_count;
        }
        dictReleaseIterator(dit);
        serverLog(LL_NOTICE, "rock key total = %d", total_rock_count);
    }
    serverAssert(ret == DICT_OK);
}


/* de is from fromDict, it may be db->dict (all key/values) or db->expire(only key) 
 * after getting the value, check it suitable, return 1 if it is OK for dumping to rock */
int _isValueSuitForDumpRock(sds key, dict *valueDict) {
    robj *val;

    dictEntry *de = dictFind(valueDict, key);
    serverAssert(de);
    val = dictGetVal(de);
    serverAssert(val);

    // stream object never evict
    if (val->type == OBJ_STREAM) return 0;

    return val->refcount == 1 ? 1: 0;
}

/* find the hotkeys first time to init 
 * if ervery hotKeys in erery db is empty (maybe after flush or never been inited before), 
 * it is the right time to init, return 0 if no need, otherwise return 1 */
int _needInitHotKeys() {
    for (int i = 0; i < server.dbnum; ++i) {
        if (dictSize(server.db[i].hotKeys)) return 0;
    }
    return 1;
}

/* if return C_OK, no need to freeMemoryIfNeeded(),
 * if return C_ERR, need to go on, referenec */
int dumpValueToRockIfNeeded() {

    if (clientsArePaused()) return C_OK;

    size_t mem_tofree;

    if (getMmemoryStateWithRock(&mem_tofree) == C_OK) return C_OK;

    /* we need try to dump some value to rocksdb for save memory */
    if (_needInitHotKeys()) initHotKeys();    

    serverLog(LL_DEBUG, "need dump rockdb, tofree = %lu", mem_tofree);

    size_t mem_freed = 0;
    int try_max_times = 0;

    long long latency = ustime();

    while (mem_freed < mem_tofree && ++try_max_times < MAX_TRY_PICK_KEY_TIMES) {
        int j, k, i;
        static unsigned int next_db = 0;
        sds bestkey = NULL;
        int bestdbid;
        redisDb *db;
        dictEntry *de;
        long long delta;

        /* different from evict, we always try to get BEST key from LRU or LFU */
        {
            struct rockPoolEntry *pool = RockPoolLRU;

            while(bestkey == NULL) {
                unsigned long total_keys = 0, keys;

                /* We don't want to make local-db choices when choosing rock keys,
                 * so to start populate the rock pool sampling keys from
                 * every DB and from their hotKeys */
                for (i = 0; i < server.dbnum; i++) {
                    db = server.db+i;
                    if ((keys = dictSize(db->hotKeys)) != 0) {
                        rockPoolPopulate(i, db->hotKeys, db->dict, pool);
                        total_keys += keys;
                    }
                }
                if (!total_keys) break; /* No keys to rock. */

                /* Go backward from best to worst element to evict. */
                for (k = RKPOOL_SIZE-1; k >= 0; k--) {
                    if (pool[k].key == NULL) continue;
                    bestdbid = pool[k].dbid;

                    de = dictFind(server.db[pool[k].dbid].dict, pool[k].key);

                    /* Remove the entry from the pool. */
                    if (pool[k].key != pool[k].cached)
                        sdsfree(pool[k].key);
                    pool[k].key = NULL;
                    pool[k].idle = 0;

                    /* If the key exists, is our pick. Otherwise it is
                     * a ghost and we need to try the next element. */
                    if (de) {
                        bestkey = dictGetKey(de);
                        break;
                    } else {
                        /* Ghost... Iterate again. */
                    }
                }
            }
        }

        /* NOTE: best key from LRU or LFU value maybe not good for dump Rock (like the value is shared obj) */
        if (bestkey && !_isValueSuitForDumpRock(bestkey, server.db[bestdbid].dict)) {
            bestkey = NULL;     // value is NOT OK, try the following random pick
        }

        /* if LRU & LFU not work,  so we need to try random alogarithm in the situation */
        if (bestkey == NULL)
        {
            /* When evicting a random key, we try to evict a key for
             * each DB, so we use the static 'next_db' variable to
             * incrementally visit all DBs. */
            for (i = 0; i < server.dbnum; i++) {
                j = (++next_db) % server.dbnum;
                db = server.db+j;
                /* we use hotKeys */
                if (dictSize(db->hotKeys)) {
                    de = dictGetRandomKey(db->hotKeys);
                    sds key = dictGetKey(de);
                    if (_isValueSuitForDumpRock(key, db->dict)) {
                        bestkey = key;
                        bestdbid = j;
                        break;
                    }
                } 
            }
        }

        if (bestkey) {
            if (dictFind(server.db[bestdbid].rockKeys, bestkey) == NULL) {
                // serverLog(LL_NOTICE, "bestkey = %s", bestkey);
                delta = (long long) zmalloc_used_memory();
                _dumpValToRock(bestkey, bestdbid);  // replace the value
                delta -= (long long) zmalloc_used_memory();
                mem_freed += delta;
            }
        } else {
            goto cant_free;
        }
    }

    if (try_max_times >= MAX_TRY_PICK_KEY_TIMES) goto cant_free;

    latency = ustime() - latency;
    serverLog(LL_DEBUG, "sucess latency(us) = %lld, try times = %d", latency, try_max_times);
    return C_OK;    // dump val to rocksdb to save enough memory

cant_free:
    if (try_max_times >= MAX_TRY_PICK_KEY_TIMES) {
        serverLog(LL_DEBUG, "try_max_times with mem_freed = %lu", mem_freed);
        latency = ustime() - latency;
        serverLog(LL_DEBUG, "fail latency(us) = %lld, try times = %d", latency, try_max_times);
    } else
        serverLog(LL_DEBUG, "bestkey NULL with mem_freed = %lu", mem_freed);
    
    return C_ERR;       /* let eviction do more help */
}


