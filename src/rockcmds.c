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
#include "rockcmds.h"

/* a lot of commands only use one key, we use one common function for all */
void cmdCheckRockForOneKey(client *c, struct redisCommand *cmd, robj **argv, int argc, list *l) {
    UNUSED(cmd);
    UNUSED(argc);
    robj *key = argv[1];
    robj *val = lookupKeyNoSideEffect(c->db, key);
    if (val == shared.valueInRock) 
        listAddNodeTail(l, key->ptr);
}

/* commands such as blpop, multi key, every arg except last arg is the keys */
void cmdCheckRockExcludeLastArg(client *c, struct redisCommand *cmd, robj **argv, int argc, list *l) {
    serverAssert(argc > 2);    // first command, last param is no key, so bigger than 2

    UNUSED(cmd);
    UNUSED(argc);

    for (int i = 1; i < argc-1; ++i) {
        robj *key = argv[i];
        robj *val = lookupKeyNoSideEffect(c->db, key);
        if (val == shared.valueInRock)
            listAddNodeTail(l, key->ptr);
    }
}

/* command like RPOPLPUSH, every param is key */
void cmdCheckRockForAllKeys(client *c, struct redisCommand *cmd, robj **argv, int argc, list *l) {
    serverAssert(argc >= 2);    

    UNUSED(cmd);
    UNUSED(argc);

    for (int i = 1; i < argc; ++i) {
        robj *key = argv[i];
        robj *val = lookupKeyNoSideEffect(c->db, key);
        if (val == shared.valueInRock)
            listAddNodeTail(l, key->ptr);
    }
}

/* command like OBJECT, first param, every thing is key */
void cmdCheckRockExcludeFirstArg(client *c, struct redisCommand *cmd, robj **argv, int argc, list *l) {
    serverAssert(argc >= 2);    // NOTE: allow no key, like command OBJECT HELP

    UNUSED(cmd);
    UNUSED(argc);

    for (int i = 2; i < argc; ++i) {
        robj *key = argv[i];
        robj *val = lookupKeyNoSideEffect(c->db, key);
        if (val == shared.valueInRock)
            listAddNodeTail(l, key->ptr);
    }
}
