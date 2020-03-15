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
#include "rock_hotkey.h"
#include "rock.h"

/* 
 * this file is for two purposes:
 * 1. how to operator hot keys -- hot keys is a dictionary in each db, 
 *    which is can be dumped to rocksdb
 * 2. how to choose hot key to dump. We use the same way as Redis
 *    using some algorithm similar to LRU/LFU, check evtion.c for reference 
 */

/* called from db.c. When key inserted, updated, delete, it may be needed 
 * to adjust the hotKey. val maybe null, if null, we need read it from db */
void addHotKeyIfNeed(redisDb *db, sds key, robj *val) {
    if (!server.alreadyInitHotKeys)
        return;     /* meaning we do not enable hotKey, we only use the check for not init (db.c) */

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
        return;      /* if value already in Rocksdb, no need */
    }  

    if (val->type == OBJ_STREAM || val->refcount == OBJ_SHARED_REFCOUNT) {
        dictDelete(db->hotKeys, key);   /* because may be overwrite */
        return;   /* we do not dump stream to Rocksdb */
    }

    dictAdd(db->hotKeys, key, NULL);    /* maybe insert a hotkey or overwrite a hotkey */
}

void deleteHotKeyIfNeed(redisDb *db, sds key) {
    if (!server.alreadyInitHotKeys) return;     /* hotKey not enable */
    dictDelete(db->hotKeys, key);       /* key may be in hotKeys */
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
        /* if there are hotKeys exisit (in otherdb), we do nothing */
        if (dictSize(server.db[i].hotKeys)) return;
    }

    /* every db hotkeys is empty and all db probably flushed, we set lazy init again */
    server.alreadyInitHotKeys = 0;  
}

/* init every hotKeys from every db's dict, the first time add keys to hotKeys
 * but there are a very edge case, all keys dupmed to Rocksdb or all keys is stream or shared object, 
 * meaning no hot keys exist, if this time, we return 0 to indicate this edge case */
int initHotKeys() {
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

/* ----------------- POOL for dump selection, like evction in Redis ------------------ */

/* please reference EVPOOL_SIZE, EVPOOL_CACHED_SDS_SIZE */
#define RKPOOL_SIZE 16
#define RKPOOL_CACHED_SDS_SIZE 255
#define MAX_TRY_PICK_KEY_TIMES 64
/* safe space assume to be 32M before eviction */
#define SAFE_MEMORY_ROCK_BEFORE_EVIC    (16<<20) 

static struct rockPoolEntry *RockPoolLRU;

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

int _getRockKeyCountInDb(int dbid) {
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
                dbid, dictSize(server.db[dbid].dict), dictSize(server.db[dbid].hotKeys), _getRockKeyCountInDb(dbid));             
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

/* estimate the memory usage considering the rockdb engine
 * return C_OK for no need to free memory by dumping to rocksdb, 
 * otherwise return C_ERR, so we need first dump memory to rocksdb first, tofree has th value
 */
int _getMmemoryStateWithRock(size_t *tofree) {
    if (!server.maxmemory) return C_OK;     /* no need limit in server config, return ASAP */

    size_t mem_reported, mem_aof_slave, mem_rock, mem_used;

    mem_reported = zmalloc_used_memory();
 
    /* mem_aof_slave is the memory used by slave buffer and AOF buffer */
    mem_aof_slave = freeMemoryGetNotCountedMemory();  
    mem_rock = getMemoryOfRock();

    mem_used = (mem_reported > mem_aof_slave + mem_rock) ? mem_reported - mem_aof_slave - mem_rock : 0;

    /* before go to evic, we need keep SAFE_MEMORY_ROCK_BEFORE_EVIC room for rock dump first */
    if (mem_used + SAFE_MEMORY_ROCK_BEFORE_EVIC <= server.maxmemory) return C_OK;

    *tofree = mem_used + SAFE_MEMORY_ROCK_BEFORE_EVIC - server.maxmemory;

    serverLog(LL_DEBUG, "maxmemory = %llu, repored =%lu, mem_aof_slave = %lu, rock = %lu, used = %lu, tofree = %lu, safe = %lu", 
        server.maxmemory, mem_reported, mem_aof_slave, mem_rock, mem_used, *tofree, (long)SAFE_MEMORY_ROCK_BEFORE_EVIC);

    return C_ERR;   /* we need to try to dump some values to rocksdb */
}


/* de is from fromDict, it may be db->dict (all key/values) or db->expire(only key) 
 * after getting the value, check it suitable, return 1 if it is OK for dumping to rock */
int _isValueSuitForDumpRock(sds key, dict *valueDict) {
    robj *val;

    dictEntry *de = dictFind(valueDict, key);
    serverAssert(de);
    val = dictGetVal(de);
    serverAssert(val);

    /* stream object never evict */
    if (val->type == OBJ_STREAM) return 0;

    return val->refcount == 1 ? 1: 0;
}

/* if return C_OK, no need to freeMemoryIfNeeded(),
 * if return C_ERR, need to go on, referenec */
int dumpValueToRockIfNeeded() {

    if (clientsArePaused()) return C_OK;

    size_t mem_tofree;

    if (_getMmemoryStateWithRock(&mem_tofree) == C_OK) return C_OK;

    /* we use lazy init for HotKeys, _getMmemoryStateWithRock() if C_OK, no need to init HotKeys */
    if (!server.alreadyInitHotKeys) {
        server.alreadyInitHotKeys = 1;  /* only once */
        int ret = initHotKeys();
        serverAssert(ret != 0);
    }

    if (!server.maxmemory_only_for_rocksdb) {
        /* we keep some hotkeys alive, i.e. not dumping all keys to Rocksdb
         * because we can try to use eviction way when not server.maxmemory_only_for_rocksdb
         * otherwise, eviction can not take effect because all lru/lfu value in Rocksdb not in memory */
        long long total_hot_keys = 0;
        for (int i = 0; i < server.dbnum; ++i) {
            total_hot_keys += dictSize(server.db[i].hotKeys);
            if (total_hot_keys <= server.maxHopeHotKeys) return C_ERR;       /* try eviction with at least 1K keys alive */
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
                delta = (long long) zmalloc_used_memory();
                dumpValToRock(bestkey, bestdbid);  /* replace the value */
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
    return C_OK;    /* dump val to rocksdb to save enough memory */

cant_free:
    if (try_max_times >= MAX_TRY_PICK_KEY_TIMES) {
        serverLog(LL_DEBUG, "try_max_times with mem_freed = %lu", mem_freed);
        latency = ustime() - latency;
        serverLog(LL_DEBUG, "fail latency(us) = %lld, try times = %d", latency, try_max_times);
    } else
        serverLog(LL_DEBUG, "bestkey NULL with mem_freed = %lu", mem_freed);
    
    return C_ERR;       /* let eviction do more help */
}
