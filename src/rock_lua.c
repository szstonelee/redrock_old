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
#include "rock_lua.h"
#include "rock.h"

/* 
 * for script(lua) we need do whole thing by one time, something like transaction
 * but transaction just accumulat some commands before 'exec' command, and if condition is OK, 
 * i.e. all rock keys restored value from db, it can be executed, and has nothing with other clients (except blocking)
 * 
 * but for script, thing is complicated
 * for every redis.call() (or redis.pcall()), we need do something like transaction, only for one client
 * but after script, some keys maybe restore from rocksdb, so it maybe influence other clients
 * we need to check it and resume these clients if condition is OK, 
 * i.e. when the clients wainting keys are just restored from the rocksdb
 * 
 * even more complicated, we have a race condition.
 * If the rocksdb thread just doing something with the ONE rock key, and the rock key is just in the script keys
 * so we need a flag in server.rockJob, it is 'alreadyFinishedByScript', 
 * and we need do some synchronized consideration 
 */

/* GLOBAL variable for script maybeFinishKey list */
static list *g_maybeFinishKeys = NULL;

void _freeMaybeKey(void *maybeKey) {
    sdsfree(((scriptMaybeKey*)maybeKey)->key);  /* because the key is duplicated */
    zfree(maybeKey);
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
            /* the key has been deleted or the value of the key not be dumped, so it need to be cleared */
            if (!_isAlreadyInScriptNeedFinishKeys(needFishishKeys, maybeKey))   /* no duplication */
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
            /* we need to check the rockJob for concurrency consideration */
            rocklock();
            if (server.rockJob.dbid == dbid) {
                if (server.rockJob.workKey) {
                    if (sdscmp(server.rockJob.workKey, key) == 0) {
                        /* workKey maybe is processed in the rockThread
                         * we need to set the alreadyFinishedByScript */
                        serverAssert(server.rockJob.alreadyFinishedByScript == 0);
                        server.rockJob.alreadyFinishedByScript = 1;
                    }
                } else if (server.rockJob.returnKey) {
                    if (sdscmp(server.rockJob.returnKey, key) == 0) {
                        /* returnKey will be processed by the main thread in future
                         * we need to set the alreadyFinishedByScript */
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

    /* because list->free is NULL, the node->val is cleared by the caller */
    listRelease(needFishishKeys);
    listRelease(zeroClients);   
}

/* when a script start, we need call this func to do some initiazation 
 * it then combined with scriptForBeforeEachCall() and scirptForBeforeExit() 
 * to do everthing related to rocksdb when it happened in script/LUA situation */
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
        checkRockForMultiCmd(c, valueInRockKeys);
    else 
        checkRockForSingleCmd(c, valueInRockKeys);

    listIter li;
    listNode *ln;
    listRewind(valueInRockKeys, &li);
    while((ln = listNext(&li))) {
        sds key = listNodeValue(ln);

        /* load the value from Rocksdb in main thread in sync mode, 
         * NOTE: maybe duplicated (i.e the value maybe restored by the previous restore, but it does no matter */
        robj *valInRock;
        doRockRestoreInMainThread(dbid, key, &valInRock);
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
    g_maybeFinishKeys = NULL;   /* ready for next script */
}
