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
#define SAFE_MEMORY_ROCK_BEFORE_EVIC    (16<<20) 
#define MAX_TRY_PICK_KEY_TIMES 64


/* called from db.c. When key inserted, updated, delete, it may be needed 
 * to adjust the hotKey. val maybe null, if null, we need read it from db */
void addHotKeyIfNeed(redisDb *db, sds key, robj *val) {
    if (!server.alreadyInitHotKeys)
        return;     // meaning we do not enable hotKey, we only use the check for not init (db.c)

    if (val == NULL) {
        dictEntry *de = dictFind(db->dict, key);
        if (de == NULL) {
            serverLog(LL_WARNING, "addHotKeyIfNeed(), de null for key = %s", key);
            return;
        }
        val = dictGetVal(de);
    }

    if (val == shared.valueInRock) {
        serverLog(LL_WARNING, "addHotKeyIfNeed(), the value is RockValue! key = %s", key);
        return;      // if value already in Rocksdb, no need  
    }  

    if (val->type == OBJ_STREAM || val->refcount == OBJ_SHARED_REFCOUNT) {
        dictDelete(db->hotKeys, key);   // because may be overwrite
        return;   // we do not dump stream to Rocksdb
    }

    dictAdd(db->hotKeys, key, NULL);    // maybe insert a hotkey or overwrite a hotkey
}

void deleteHotKeyIfNeed(redisDb *db, sds key) {
    if (!server.alreadyInitHotKeys) return;     // hotKey not enable
    dictDelete(db->hotKeys, key);       // key may be in hotKeys
}

void _rock_debug_print_key_report() {
    dictEntry *de;
    dictIterator *di;

    long long all_total = 0;
    int max_other_print =  3;
    int other_count = 0;
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
                if (other_count <= max_other_print) {
                    serverLog(LL_NOTICE, "no-rock-key: key = %s, val type = %d, encoding = %d, refcount = %d", 
                        dictGetKey(de), o->type, o->encoding, o->refcount);
                }
            }
        }
        dictReleaseIterator(di);

        all_total += total;
        if (total) {
            int rockPercent = rock * 100 / total;
            serverLog(LL_NOTICE, "db=%d, key total = %ld, value in rock = %ld, rock percentage = %d%%, hot key = %lu, shared = %lu, stream = %lu", 
                i, total, rock, rockPercent, dictSize(server.db[i].hotKeys), share, stream);
        }
    }
    serverLog(LL_NOTICE, "all db key total = %lld", all_total);
}

void _print_rock_key(long limit) {
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
    } else if (strcmp(echoStr, "testrdbservice") == 0) {
        _test_rdb_service();
    } else if (strcmp(echoStr, "rockkey") == 0) {
        long long keyLimit = 0;
        if (c->argc > 2) 
            getLongLongFromObject(c->argv[2], &keyLimit);
        if (keyLimit <= 0) keyLimit = 1;
        _print_rock_key(keyLimit);
    } else if (strcmp(echoStr, "report") == 0) {
        _rock_debug_print_key_report();
    } else if (strcmp(echoStr, "debuglru") == 0) {
        _debug_lru();
    }

    addReplyBulk(c,c->argv[1]);
}

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

/* init every hotKeys from every db's dict, the first time add keys to hotKeys
 * but there are a very edge case, all keys dupmed to Rocksdb or all keys is stream or shared object, 
 * meaning no hot keys exist, if this time, we return 0 to indicate this edge case */
int _initHotKeys() {
    dictEntry *de;
    int ret = 0;
    for (int i = 0; i < server.dbnum; ++i) {
        serverAssert(dictSize(server.db[i].hotKeys) == 0);
        dictIterator *dit = dictGetIterator(server.db[i].dict);
        while ((de = dictNext(dit))) 
            addHotKeyIfNeed(&server.db[i], dictGetKey(de), dictGetVal(de));
        dictReleaseIterator(dit);
        if (dictSize(server.db[i].hotKeys)) ret = 1;
    }
    return ret;
}

static struct rockPoolEntry *RockPoolLRU;

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
        // We need to release the resource allocated by the following code in previous timing
        sdsfree(server.rockJob.returnKey);   

    // NOTE: workKey must be a copy of key, becuase scripts may be delete the rockKeys
    // SO we need to release it by ourself in the above codes
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

int _isAlreadyInScriptNeedFinishKeys(list *l, scriptMaybeKey *maybeKey) {
    serverAssert(maybeKey != NULL);

    listIter li;
    listNode *ln;

    listRewind(l, &li);
    while((ln = listNext(&li))) {
        scriptMaybeKey *check = listNodeValue(ln);
        serverAssert(check);
        if (check->dbid == maybeKey->dbid && sdscmp(check->key, maybeKey->key) == 0) 
            return 1;
    }

    return 0;
}

/* when script finished, we need to scan the maybe-finished key, which is restore from rocksdb,
 * 'maybe' because for one command in script, the key is restored, but after that(by the command or other command followed it), 
 * the key maybe dumped to rocksdb again. So we need to check it again. And there maybe duplicated keys
 * so we need to remove the duplicated key because we can only resume the client once */
void _clearMaybeFinishKeyFromScript(list *maybeKeys) {
    listIter li;
    listNode *ln;

    list *needFishishKeys = listCreate();

    listRewind(maybeKeys, &li);
    while((ln = listNext(&li))) {
        scriptMaybeKey *maybeKey = listNodeValue(ln);
        sds key = maybeKey->key;
        int dbid = maybeKey->dbid;

        dictEntry *entry = dictFind(server.db[dbid].dict, key);
        if (!entry || dictGetVal(entry) != shared.valueInRock) {
            // the key has been deleted or the value of the key not be dumped, so it need to be cleared
            if (!_isAlreadyInScriptNeedFinishKeys(needFishishKeys, maybeKey))   // no duplication 
                listAddNodeTail(needFishishKeys, maybeKey); 
        } 
    }

    list *zeroClients = listCreate();
    listRewind(needFishishKeys, &li);
    while((ln = listNext(&li))) {
        scriptMaybeKey *scriptKey = listNodeValue(ln);
        sds key = scriptKey->key;
        int dbid = scriptKey->dbid;
        list *clients =  dictFetchValue(server.db[dbid].rockKeys, key);
        if (clients) {
            listIter cli;
            listNode *cln;
            listRewind(clients, &cli);
            while((cln = listNext(&cli))) {
                client *c = listNodeValue(cln);
                serverAssert(c && c->rockKeyNumber > 0);
                --c->rockKeyNumber;
                if (c->rockKeyNumber == 0)
                    listAddNodeTail(zeroClients, c);
            }
            // we need to check the rockJob for concurrency consideration
            rocklock();
            if (server.rockJob.dbid == dbid) {
                if (server.rockJob.workKey) {
                    if (sdscmp(server.rockJob.workKey, key) == 0) {
                        // workKey maybe is processed in the rockThread
                        // we need to set the alreadyFinishedByScript
                        serverAssert(server.rockJob.alreadyFinishedByScript == 0);
                        server.rockJob.alreadyFinishedByScript = 1;
                    }
                } else if (server.rockJob.returnKey) {
                    if (sdscmp(server.rockJob.returnKey, key) == 0) {
                        // returnKey will be processed by the main thread in future
                        // we need to set the alreadyFinishedByScript
                        serverAssert(server.rockJob.alreadyFinishedByScript == 0);
                        server.rockJob.alreadyFinishedByScript = 1;
                    }
                }
            }
            rockunlock();
        }
        /* delete the entry in rockKeys, NOTE: the key maybe NOT in rockKeys, different from  _clearFinishKey() */
        dictDelete(server.db[dbid].rockKeys, key);
    }

    /* try resume the zeroClients */
    listRewind(zeroClients, &li);
    while((ln = listNext(&li))) {
        client *c = listNodeValue(ln);
        checkThenResumeRockClient(c);
    }

    // because list->free is NULL, the node->val is cleared by the caller
    listRelease(needFishishKeys);
    listRelease(zeroClients);   
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

void _restore_obj_from_rocksdb(int dbid, sds key, robj **val, int fromMainThread) {
    void *val_db_ptr;
    size_t val_db_len;

    rocksdbapi_read(dbid, key, sdslen(key), &val_db_ptr, &val_db_len);

    if (val_db_ptr == NULL) {
        // fromMainThread meaning it is from script/rdb fork(), 
        // otherwise it means from RockThread
        serverLog(LL_WARNING, "_restore_obj_from_rocksdb(), fromMainThread = %d, subChild = %d, key = %s, ", 
            fromMainThread, server.inSubChildProcessState, key);
        serverPanic("_restore_obj_from_rocksdb()");
        return;
    }
    
    // robj *o = desString(val_db_ptr, val_db_len);
    robj *o = desObject(val_db_ptr, val_db_len);
    serverAssert(o);

    // we need to free the memory allocated by rocksdbapi
    zfree(val_db_ptr);

    *val = o;   
}

void _doRockJobInRockThread(int dbid, sds key, robj **val) {
    _restore_obj_from_rocksdb(dbid, key, val, 0);
}

/* because script call */
void _doRockRestoreInMainThread(int dbid, sds key, robj **val) {
    _restore_obj_from_rocksdb(dbid, key, val, 1);
}

// GLOBAL variable for script maybeFinishKey list
static list *g_maybeFinishKeys = NULL;

void _freeMaybeKey(void *maybeKey) {
    sdsfree(((scriptMaybeKey*)maybeKey)->key);  // because the key is duplicated
    zfree(maybeKey);
}

/* when a script start, we need call this func to do some initiazation 
 * it then combined with scriptForBeforeEachCall() and scirptForBeforeExit() 
 * to do everthing related to rocksdb when it happened in script/LUA situation*/
void scriptWhenStartForRock() {
    serverAssert(g_maybeFinishKeys == NULL);
    g_maybeFinishKeys = listCreate();
    g_maybeFinishKeys->free = _freeMaybeKey;  // free each key in scirptForBeforeExit()
}

/* every redis call() when a script is executing, need to check the key in Rocksdb, 
 * if value in rocksdb, we need restore it in main thread later by scirptForBeforeExitForRock(), 
 * side effects: add the restored keys to g_maybeKeys */
void scriptForBeforeEachCallForRock(client *c) {
    int dbid = c->db->id;
    serverAssert(c);
    serverAssert(c->rockKeyNumber == 0);

    list *valueInRockKeys;  /* list of sds keys */

    valueInRockKeys = listCreate();

    /* 1. check whether there are any key's value in Rocksdb */
    if (c->flags & CLIENT_MULTI) 
        _checkRockForMultiCmd(c, valueInRockKeys);
    else 
        _checkRockForSingleCmd(c, valueInRockKeys);

    listIter li;
    listNode *ln;
    listRewind(valueInRockKeys, &li);
    while((ln = listNext(&li))) {
        sds key = listNodeValue(ln);

        // load the value from Rocksdb in main thread in sync mode, 
        // NOTE: maybe duplicated (i.e the value maybe restored by the previous restore, but it does no matter
        robj *valInRock;
        _doRockRestoreInMainThread(dbid, key, &valInRock);
        dictEntry *de = dictFind(c->db->dict, key);
        robj *checkValue = dictGetVal(de);
        if (checkValue == shared.valueInRock)
            dictSetVal(server.db[dbid].dict, de, valInRock);
        else
            decrRefCount(valInRock);          

        scriptMaybeKey *maybeFinishKey = zmalloc(sizeof(scriptMaybeKey));
        maybeFinishKey->dbid = dbid;
        sds copy = sdsdup(key);
        maybeFinishKey->key = copy;
        listAddNodeTail(g_maybeFinishKeys, maybeFinishKey);
    }

    listRelease(valueInRockKeys);
}

/* every script exit no matter successfully or failly, 
 * need to call this to resume other clients for those keys restored from rocksdb to memory */
void scirptForBeforeExitForRock() {
    _clearMaybeFinishKeyFromScript(g_maybeFinishKeys);
    listRelease(g_maybeFinishKeys);
    g_maybeFinishKeys = NULL;   // ready for next script
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
        // if the job has already finished by script, we do not need to clear finish key
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
    // We tempory call the func for rdb test
    // but it is not correct, because rdb is in another pid
    // in futrue, we need client/server model to solve the problem 

    robj *o;

    if (server.inSubChildProcessState) {
        // serverLog(LL_NOTICE, "In subprocess envirenmont");
        // because we are in child process to get rocksdb val, we need a client/server mode
        sds val = requestSnapshotValByKeyInRdbProcess(dbid, key, server.rockRdbParams);
        if (val) {
            o = desObject(val, sdslen(val));
            sdsfree(val);
        } else {
            o = NULL;
            serverLog(LL_WARNING, "Rocksdb string val is null in child process!");
        }
    } else {    
        // for call in sync way in main thread of main process, like 'save' command 
        _doRockRestoreInMainThread(dbid, key, &o);
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
    serverAssert(!server.inSubChildProcessState);   // In child process for rdb-save, we can not dump value 

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

/* when flushdb, db will be empted, check db.c */
void clearHotKeysWhenEmptyDb(redisDb *db) {
    if(!server.alreadyInitHotKeys) 
    {
        serverAssert(dictSize(db->hotKeys) == 0);
        return;
    }

    if (dictSize(db->hotKeys) == 0) return;

    dictEmpty(db->hotKeys, NULL);

    for (int i = 0; i < server.dbnum; ++i) {
        // if there are hotKeys exisit (in otherdb), we do nothing
        if (dictSize(server.db[i].hotKeys)) return;
    }

    // every db hotkeys is empty and all db probably flushed, we set lazy init again
    server.alreadyInitHotKeys = 0;  
}

/* if return C_OK, no need to freeMemoryIfNeeded(),
 * if return C_ERR, need to go on, referenec */
int dumpValueToRockIfNeeded() {

    if (clientsArePaused()) return C_OK;

    size_t mem_tofree;

    if (getMmemoryStateWithRock(&mem_tofree) == C_OK) return C_OK;

    /* we use lazy init for HotKeys, getMmemoryStateWithRock() if C_OK, no need to init HotKeys */
    if (!server.alreadyInitHotKeys) {
        server.alreadyInitHotKeys = 1;  // only once
        int ret = _initHotKeys();
        serverAssert(ret != 0);
    }

    if (!server.maxmemory_only_for_rocksdb) {
        // we keep some hotkeys alive, i.e. not dumping all keys to Rocksdb
        // because we can try to use eviction way when not server.maxmemory_only_for_rocksdb
        // otherwise, eviction can not take effect because all lru/lfu value in Rocksdb not in memory
        long long total_hot_keys = 0;
        for (int i = 0; i < server.dbnum; ++i) {
            total_hot_keys += dictSize(server.db[i].hotKeys);
            if (total_hot_keys <= server.maxHopeHotKeys) return C_ERR;       // try eviction with at least 1K keys alive
        }
    }

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


