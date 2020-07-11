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

#include "server.h"
#include "rock.h"
#include "rocksdbapi.h"
#include "rock_serdes.h"
#include "rock_hotkey.h"
#include "rock_rdb.h"

#if defined(__APPLE__)
    #include <os/lock.h>
    static os_unfair_lock spinLock;

    void initSpinLock() {
        spinLock = OS_UNFAIR_LOCK_INIT;
    }
#else
    #include <pthread.h>
    static pthread_spinlock_t spinLock;

    void initSpinLock() {
        pthread_spin_init(&spinLock, 0);
    }    
#endif

#define ROCK_THREAD_MAX_SLEEP_IN_US 1024

/* 
 * This is the core module for RedRock. 
 * It uses a standalone rock thread for reading disk, i.e. Rocksdb. 
 * So we need something to synchronize.
 * This time, we use spin lock because we want the master thread run as fast as possiple.
 * The master thread has more priority/time for executuation, i.e. no sleep.
 * But the rock thread has some chance to sleep, i.e. when not busy. If busy, rock thread does not sleep.
 * When master thread found one command from one client will read value from disk,
 * it put the client in frozen mode, and add a rock job for the rock thread to read from disk.
 * After the rock thread finish the disk job, it give a signal to master thread by pipe
 * How about write? 
 * We let write to be done in master thread, because for Rcoksdb, writing is to memory directly(Memtable) not to disk
 */

void _rockKeyReport() {
    dictEntry *de;
    dictIterator *di;

    long long all_total = 0;
    long long other_count = 0;
    for (int i = 0; i < server.dbnum; ++i) {
        long total = 0, rock = 0, share = 0, stream = 0;
        int print_one_key = 0;

        di = dictGetIterator(server.db[i].dict);
        while ((de = dictNext(di))) {
            ++total;
            robj *o = dictGetVal(de);
            if (o == shared.valueInRock) {
                ++rock;
                if (!print_one_key) {
                    sds key = dictGetKey(de);
                    print_one_key = 1;
                    serverLog(LL_NOTICE, "db=%d, at lease one rock key = %s", i, key);
                }
                sds key = dictGetKey(de);
                dictEntry *check = dictFind(server.db[i].hotKeys, key);
                if (check != NULL) 
                    serverLog(LL_WARNING, "something wrong, rock key in hotKeys! key = %s", key);
            } else if (o->refcount == OBJ_SHARED_REFCOUNT) {
                ++share;
            } else if (o->type == OBJ_STREAM) {
                ++stream;
            } else {
                ++other_count;
            }
        }
        dictReleaseIterator(di);

        all_total += total;
        if (total) {
            int rockPercent = rock * 100 / total;
            serverLog(LL_NOTICE, "db=%d, key total = %ld, rock key = %ld, percentage = %d%%, hot key = %lu, shared = %lu, stream = %lu", 
                i, total, rock, rockPercent, dictSize(server.db[i].hotKeys), share, stream);
        }
    }
    serverLog(LL_NOTICE, "all db key total = %lld, other total = %lld", all_total, other_count);
}

void _printRockKeys(long limit) {
    dictEntry *de;
    dictIterator *di;

    long long count = 0;
    for (int i = 0; i < server.dbnum; ++i) {
        long long db_count = 0;
        di = dictGetIterator(server.db[i].dict);
        while ((de = dictNext(di))) {
            robj *o = dictGetVal(de);
            if (o == shared.valueInRock) {
                serverLog(LL_NOTICE, "%s", dictGetKey(de));
                ++count;
                ++db_count;
                if (count >= limit) break;
            }
        }
        dictReleaseIterator(di);
        if (db_count)
            serverLog(LL_NOTICE, "the above rock key belongs to dbid = %d, total = %lld", i, db_count);
    }
}

void _debug_lru() {
    int db_count = 0, rock_count = 0, hot_count = 0;
    for (int i = 0; i< 1000; ++i) {
        sds key = sdsfromlonglong(i);
        dictEntry *de = dictFind(server.db[0].dict, key);
        if (de) {
            ++db_count;
            robj *val = dictGetVal(de);
            if (val == shared.valueInRock)
                ++rock_count;
        }
        de = dictFind(server.db[0].hotKeys, key);
        if (de) ++hot_count;
        sdsfree(key);
    }
    serverLog(LL_NOTICE, "db_count = %d, rock_count = %d, hot_count = %d", 
        db_count, rock_count, hot_count);
}

void rockCommand(client *c) {
    serverLog(LL_NOTICE, "rock command!");

    char *echoStr = c->argv[1]->ptr;
    if (strcmp(echoStr, "testserdesstr") == 0) {
        _test_ser_des_string();
    } else if (strcmp(echoStr, "testserdeslist") == 0) {
        _test_ser_des_list();
    } else if (strcmp(echoStr, "testserdesset") == 0) {
        _test_ser_des_set();
    } else if (strcmp(echoStr, "testserdeshash") == 0) {
        _test_ser_des_hash();
    } else if (strcmp(echoStr, "testserdeszset") == 0) {
        _test_ser_des_zset();
    } else if (strcmp(echoStr, "rockmem") == 0) {
        size_t mem_usage = getMemoryOfRock();
        serverLog(LL_NOTICE, "rock mem usage = %lu", mem_usage);
    } else if (strcmp(echoStr, "rockkey") == 0) {
        long long keyLimit = 0;
        if (c->argc > 2) 
            getLongLongFromObject(c->argv[2], &keyLimit);
        if (keyLimit <= 0) keyLimit = 1;
        _printRockKeys(keyLimit);
    } else if (strcmp(echoStr, "report") == 0) {
        _rockKeyReport();
    } else if (strcmp(echoStr, "debuglru") == 0) {
        _debug_lru();
    }

    addReplyBulk(c,c->argv[1]);
}

/* check whether the config is enabled 
 * NOTE: we set these config parameter is mutable, but actually it can not be changed online 
 * return 1 if enabled, otherwise 0 */
int isRockFeatureEnabled() {
    if (server.enable_rocksdb_feature && server.maxmemory > 0) 
        return 1;
    else
        return 0;
}

/* error return -1, 1 init ok */
int initRocksdb(int dbnum, char *path) {
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

void teardownRocksdb() {
    rocksdbapi_teardown();
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
    if (server.rockJob.workKey) sdsfree(server.rockJob.workKey);
    server.rockJob.workKey = NULL;
    if (server.rockJob.returnKey) sdsfree(server.rockJob.returnKey);
    server.rockJob.returnKey = NULL;
    server.rockJob.valInRock = NULL;
    server.rockJob.alreadyFinishedByScript = 0;
    rockunlock();
}

int _isZeroRockJobState() {
    int ret;
    rocklock();
    if (server.rockJob.dbid == -1) {
        ret = 1;
        serverAssert(server.rockJob.returnKey == NULL && server.rockJob.workKey == NULL);
    } else {
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

    if (server.rockJob.returnKey) 
        /* We need to release the resource allocated by the following code in previous timing */
        sdsfree(server.rockJob.returnKey);   

    /* NOTE: workKey must be a copy of key, becuase scripts may be delete the rockKeys
     * SO we need to release it by ourself in the above codes */
    sds copyKey = sdsdup(key);
    server.rockJob.workKey = copyKey;

    /* Note: before this point, returnKey maybe not be NULL */
    server.rockJob.dbid = dbid;
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
         * NOTE: do not use key because it may be freed later by the caller, use dictGetKey(entry) */
        int ret = dictAdd(server.db[dbid].hotKeys, dictGetKey(entry), NULL);        
        serverAssert(ret == DICT_OK);
    } /* else due to deleted, updated or flushed */

    /* adjust rockKeyNumber and get zeroClients
     * NOTE: clients maybe empty, because client may be disconnected */
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

/* most single command need this check */
void checkRockForSingleCmd(client *c, list *l) {
    if (c->cmd->checkRockProc) 
        c->cmd->checkRockProc(c, c->cmd, c->argv, c->argc, l);
}

/* command like exec (transaction), or script context */
void checkRockForMultiCmd(client *c, list *l) {
    for (int j = 0; j < c->mstate.count; ++j) {
        if (c->mstate.commands[j].cmd->checkRockProc) 
            c->mstate.commands[j].cmd->checkRockProc(
                c, c->mstate.commands[j].cmd, 
                c->mstate.commands[j].argv, c->mstate.commands[j].argc, l);
    }
}

void _restore_obj_from_rocksdb(int dbid, sds key, robj **val, int fromMainThread) {
    void *val_db_ptr;
    size_t val_db_len;

    rocksdbapi_read(dbid, key, sdslen(key), &val_db_ptr, &val_db_len);

    if (val_db_ptr == NULL) {
        /* fromMainThread meaning it is from script/rdb fork(), 
         * otherwise it means from RockThread */
        serverLog(LL_WARNING, "_restore_obj_from_rocksdb(), fromMainThread = %d, subChild = %d, key = %s, ", 
            fromMainThread, server.inSubChildProcessState, key);
        serverPanic("_restore_obj_from_rocksdb()");
        return;
    }
    
    robj *o = desObject(val_db_ptr, val_db_len);
    serverAssert(o);

    /* we need to free the memory allocated by rocksdbapi */
    zfree(val_db_ptr);

    *val = o;   
}

void _doRockJobInRockThread(int dbid, sds key, robj **val) {
    _restore_obj_from_rocksdb(dbid, key, val, 0);
}

/* because script call or rdb aof stuff */
void doRockRestoreInMainThread(int dbid, sds key, robj **val) {
    _restore_obj_from_rocksdb(dbid, key, val, 1);
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
    int alreadyFininshedByScript;

    /* deal with return result */
    rocklock();
    serverAssert(server.rockJob.dbid != -1);
    serverAssert(server.rockJob.workKey == NULL);
    serverAssert(server.rockJob.returnKey != NULL);
    serverAssert(server.rockJob.valInRock != NULL);
    finishDbid = server.rockJob.dbid;
    finishKey = server.rockJob.returnKey;
    val = server.rockJob.valInRock;
    alreadyFininshedByScript = server.rockJob.alreadyFinishedByScript;
    rockunlock();

    if (!alreadyFininshedByScript)
        /* if the job has already finished by script, we do not need to clear finish key */
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

/* for rdb & aof backup  */
robj* loadValFromRockForRdb(int dbid, sds key) {
    /* We tempory call the func for rdb test
     * but it is not correct, because rdb is in another pid
     * in futrue, we need client/server model to solve the problem */
    robj *o;

    if (server.inSubChildProcessState) {
        /* serverLog(LL_NOTICE, "In subprocess envirenmont");
         * because we are in child process to get rocksdb val, we need a client/server mode */
        sds val = requestSnapshotValByKeyInRdbProcess(dbid, key, server.rockRdbParams);
        if (val) {
            o = desObject(val, sdslen(val));
            sdsfree(val);
        } else {
            o = NULL;
            serverLog(LL_WARNING, "Rocksdb string val is null in child process!");
        }
    } else {    
        /* for call in sync way in main thread of main process, like 'save' command */
        doRockRestoreInMainThread(dbid, key, &o);
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
void* _mainProcessInRockThread(void *arg) {
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
        checkRockForMultiCmd(c, valueInRockKeys);
    else 
        checkRockForSingleCmd(c, valueInRockKeys);
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

size_t getMemoryOfRock() {
    if (!isRockFeatureEnabled()) return 0;

    return rocksdbapi_memory();
}

void dumpValToRock(sds key, int dbid) {
    serverAssert(!server.inSubChildProcessState);   /* In child process for rdb-save, we can not dump value */

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





