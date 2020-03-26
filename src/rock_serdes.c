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
#include "intset.h"
#include "rock.h"

/* This is for serialize/deserilize all data types of Redis,
 * like String, Set, Hash, List, Zset
 * 
 * We do not serialize Stream becasue it is used ofen and need instance response
 * 
 * Redis use its coded way for persistence, so it serialize more type, like LUA scripts
 * But for Rocksdb, it is temporary persistence, so it does not nedd to consider 
 * 1. big/small endiness
 * 2. LUA object/ Stream Object, which is temporary or instant object
 * 
 * The effiency for encoding the data types may be not as good as rdb encoding, 
 * but I have no time for optimazation, i.e. the rdb encoding is too complicated for me */

/* serializa robj type to sds, reference rdbSaveObjectType() */
sds serObjectType(robj *o) {
    char rockType;
    switch (o->type) {
    case OBJ_STRING:
        rockType = ROCK_TYPE_STRING;
        break;
    case OBJ_LIST:
        if (o->encoding == OBJ_ENCODING_QUICKLIST) {
            rockType = ROCK_TYPE_LIST_QUICKLIST;
            break;
        } else
            serverPanic("Unknown list encoding");

    case OBJ_SET:
        if (o->encoding == OBJ_ENCODING_INTSET) {
            rockType = ROCK_TYPE_SET_INTSET;
            break;
        } else if (o->encoding == OBJ_ENCODING_HT) {
            rockType = ROCK_TYPE_SET_HT;
            break;
        } else
            serverPanic("Unknown set encoding");

    case OBJ_ZSET:
        if (o->encoding == OBJ_ENCODING_ZIPLIST) {
            rockType = ROCK_TYPE_ZSET_ZIPLIST;
            break;
        } else if (o->encoding == OBJ_ENCODING_SKIPLIST) {
            rockType = ROCK_TYPE_ZSET_SKIPLIST;
            break;
        } else
            serverPanic("Unknown sorted set encoding");

    case OBJ_HASH:
        if (o->encoding == OBJ_ENCODING_ZIPLIST) {
            rockType = ROCK_TYPE_HASH_ZIPLIST;
            break;
        } else if (o->encoding == OBJ_ENCODING_HT) {
            rockType = ROCK_TYPE_HASH_HT;
            break;
        } else
            serverPanic("Unknown hash encoding");

    default:
        serverPanic("Unknown object type");
    }

    return sdsnewlen(&rockType, 1);
}

/* serialize string object, append it to dst and return the new dst 
 * reference rdb.c rdbSaveStringObject() */
sds serString(sds dst, robj *o) {
    char encoding = o->encoding;
    if (encoding == OBJ_ENCODING_INT) {
        dst = sdscatlen(dst, &encoding, 1);
        long long val = (long long)o->ptr;
        dst = sdscatlen(dst, &val, 8);

    } else {
        serverAssert(encoding == OBJ_ENCODING_RAW || encoding == OBJ_ENCODING_EMBSTR);
        encoding = o->encoding;
        dst = sdscatlen(dst, &encoding, 1);
        dst = sdscatlen(dst, o->ptr, sdslen(o->ptr));
    }
    return dst;
}

/* descerialize buffer to robj when the buffer from rocksdb is String Type 
 * please reference the above serString() for reference */
#define OBJ_ENCODING_EMBSTR_SIZE_LIMIT 44
robj *desString(char *s, size_t len, uint32_t lru) {
    serverAssert(len >= 2+sizeof(lru));
    serverAssert(s[0] == ROCK_TYPE_STRING);
    s += 1+sizeof(lru);
    len -= 1+sizeof(lru);

    robj *o;
    int encoding = (int)(*s);
    ++s;
    --len;

    if (encoding == OBJ_ENCODING_INT) {
        serverAssert(len == 8);
        o = createStringObjectFromLongLongForValue(*(long long*)s);
    } else {
        serverAssert(encoding == OBJ_ENCODING_RAW || encoding == OBJ_ENCODING_EMBSTR);
        if (encoding == OBJ_ENCODING_RAW) {
            o = createRawStringObject(s, len);
        } else {
            serverAssert(len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT);
            o = createEmbeddedStringObject(s, len);
        }
    }
    o->lru = lru;
    return o;
}

/* serialize string object, append it to dst and return the new dst 
 * reference rdb.c rdbSaveObject() */
sds serList(sds dst, robj *o) {
    serverAssert(o->type == OBJ_LIST);
    serverAssert(o->encoding == OBJ_ENCODING_QUICKLIST);

    quicklist *ql = o->ptr;    

    quicklistIter *qit = quicklistGetIterator(ql, AL_START_HEAD);
    quicklistEntry entry;

    while(quicklistNext(qit, &entry)) {
        if (entry.value) {
            unsigned int len = entry.sz;
            dst = sdscatlen(dst, &len, sizeof(unsigned int));
            dst = sdscatlen(dst, entry.value, len);
        } else {
            sds str = sdsfromlonglong(entry.longval);
            unsigned int len = (unsigned int)sdslen(str);
            dst = sdscatlen(dst, &len, sizeof(unsigned int));
            dst = sdscatlen(dst, str, len);
            sdsfree(str);
        }
    }

    quicklistReleaseIterator(qit);

    return dst;
}

/* reference rdb.c rdbLoadObject() and the above serList() */
robj *desList(char *s, size_t len, uint32_t lru) {    
    serverAssert(len >= 1+sizeof(uint32_t));
    serverAssert(s[0] == ROCK_TYPE_LIST_QUICKLIST);
    s += 1+sizeof(uint32_t);
    len -= 1+sizeof(uint32_t);

    robj *list = createQuicklistObject();
    quicklistSetOptions(list->ptr, server.list_max_ziplist_size,
                        server.list_compress_depth);

    while (len) {
        serverAssert(len >= sizeof(unsigned int));
        unsigned int entry_len = *((unsigned int*)s);
        s += sizeof(unsigned int);
        len -= sizeof(unsigned int);
        serverAssert(len >= entry_len);
        quicklistPushTail(list->ptr, s, entry_len);
        s += entry_len;
        len -= entry_len;
    }

    list->lru = lru;
    return list;
}

/* reference rdb.c rdbSaveObject() */
sds serSet(sds dst, robj *o) {
    serverAssert(o->type == OBJ_SET);

    if (o->encoding == OBJ_ENCODING_INTSET) {
        intset *is = o->ptr;
        dst = sdscatlen(dst, &(is->encoding), sizeof(uint32_t));
        dst = sdscatlen(dst, &(is->length), sizeof(uint32_t));
        size_t content_len = is->encoding * is->length;
        dst = sdscatlen(dst, is->contents, content_len);

    } else if (o->encoding == OBJ_ENCODING_HT) {
        dict *set = o->ptr;
        dictIterator *di = dictGetIterator(set);
        dictEntry* de;
        size_t count = dictSize(set);
        dst = sdscatlen(dst, &count, sizeof(size_t));
        while ((de = dictNext(di))) {
            sds ele = dictGetKey(de);
            size_t ele_len = sdslen(ele);
            dst = sdscatlen(dst, &ele_len, sizeof(size_t));
            dst = sdscatlen(dst, ele, ele_len);
        }
        dictReleaseIterator(di);

    } else 
        serverPanic("serSet()!");

    return dst;    
}

/* reference rdb.c rdbLoadObject() and serSet() */
robj *desSet(char *s, size_t len, uint32_t lru) {
    serverAssert(len >= 1 + sizeof(uint32_t));
    int set_rock_type = *s;
    s += 1+sizeof(uint32_t);
    len -= 1+sizeof(uint32_t);

    robj *o = NULL;
    if (set_rock_type == ROCK_TYPE_SET_INTSET) {
        o = createIntsetObject();
        intset *is = (intset*)(o->ptr);
        serverAssert(len >= sizeof(uint32_t));
        uint32_t is_encoding = *((uint32_t*)s);
        is->encoding = is_encoding;
        s += sizeof(uint32_t);
        len -= sizeof(uint32_t);
        
        serverAssert(len >= sizeof(uint32_t));
        uint32_t is_length = *((uint32_t*)s);
        is->length = is_length;
        /* reference intset.c intsetResize() */
        size_t content_sz = is_length*is_encoding;
        is = zrealloc(is,sizeof(intset)+content_sz);
        o->ptr = is;
        s += sizeof(uint32_t);
        len -= sizeof(uint32_t);

        serverAssert(len == is_encoding * is_length);
        memcpy(is->contents, s, len);

    } else if (set_rock_type == ROCK_TYPE_SET_HT){
        serverAssert(len >= sizeof(size_t));
        size_t count = *((size_t*)s);
        s += sizeof(size_t);
        len -= sizeof(size_t);

        o = createSetObject();
        if (count > DICT_HT_INITIAL_SIZE)
            dictExpand(o->ptr, count);

        while (len) {
            serverAssert(len >= sizeof(size_t));
            size_t ele_len;
            ele_len = *((size_t*)s);
            // serverAssert(ele_len != 0);      // NOTE: could be empty string
            s += sizeof(size_t);
            len -= sizeof(size_t);
            
            serverAssert(len >= ele_len);
            sds sdsele = sdsnewlen(s, ele_len);
            dictAdd(o->ptr, sdsele, NULL);
            s += ele_len;
            len -= ele_len;

            --count;
        }
        serverAssert(count == 0);

    } else {
        serverPanic("desSet()");
    }

    o->lru = lru;
    return o;
}

/* reference rdb.c rdbSaveObject() */
sds serHash(sds dst, robj *o) {
    serverAssert(o->type == OBJ_HASH);

    if (o->encoding == OBJ_ENCODING_ZIPLIST) {
        size_t zip_list_bytes_len = ziplistBlobLen(o->ptr);
        dst = sdscatlen(dst, &zip_list_bytes_len, sizeof(size_t));
        dst = sdscatlen(dst, o->ptr, zip_list_bytes_len);

    } else if (o->encoding == OBJ_ENCODING_HT) {
        dict *hash = o->ptr;
        dictIterator *di = dictGetIterator(hash);
        dictEntry* de;
        size_t count = dictSize(hash);
        dst = sdscatlen(dst, &count, sizeof(size_t));
        while ((de = dictNext(di))) {
            sds field = dictGetKey(de);            
            size_t field_len = sdslen(field);            
            dst = sdscatlen(dst, &field_len, sizeof(size_t));
            dst = sdscatlen(dst, field, field_len);

            sds val = dictGetVal(de);
            size_t val_len = sdslen(val);
            dst = sdscatlen(dst, &val_len, sizeof(size_t));
            dst = sdscatlen(dst, val, val_len);
        }
        dictReleaseIterator(di); 

    } else {
        serverPanic("serHash()");
    }

    return dst; 
}

/* reference rdb.c rdbLoadObject() and serHash() */
robj *desHash(char *s, size_t len, uint32_t lru) {
    serverAssert(len >= 1+sizeof(uint32_t));
    int hash_rock_type = *s;
    s += 1+sizeof(uint32_t);
    len -= 1+sizeof(uint32_t);

    robj *o = NULL;
    if (hash_rock_type == ROCK_TYPE_HASH_ZIPLIST) {
        size_t zip_list_bytes_len = *((size_t*)s);
        s += sizeof(size_t);
        len -= sizeof(size_t);
        serverAssert(len == zip_list_bytes_len);

        char *ziplist = zmalloc(zip_list_bytes_len);
        memcpy(ziplist, s, zip_list_bytes_len);

        o = createObject(OBJ_HASH, ziplist);
        o->encoding = OBJ_ENCODING_ZIPLIST;

    } else if (hash_rock_type == ROCK_TYPE_HASH_HT) {
        serverAssert(len >= sizeof(size_t));
        size_t count = *((size_t*)s);
        s += sizeof(size_t);
        len -= sizeof(size_t);

        dict *dict_internal = dictCreate(&hashDictType, NULL);
        if (count > DICT_HT_INITIAL_SIZE)
            dictExpand(dict_internal, count);
       
        while (len) {
            serverAssert(len >= sizeof(size_t));
            size_t field_len = *((size_t*)s);
            s += sizeof(size_t);
            len -= sizeof(size_t);

            serverAssert(len >= field_len);
            sds field = sdsnewlen(s, field_len);
            s += field_len;
            len -= field_len;

            serverAssert(len >= sizeof(size_t));
            size_t val_len = *((size_t*)s);
            s += sizeof(size_t);
            len -= sizeof(size_t);

            serverAssert(len >= val_len);
            sds val = sdsnewlen(s, val_len);
            s += val_len;
            len -= val_len;

            int ret = dictAdd(dict_internal, field, val);
            serverAssert(ret == DICT_OK);

            --count;
        }
        serverAssert(count == 0);

        o = createObject(OBJ_HASH, dict_internal);
        o->encoding = OBJ_ENCODING_HT;
    } else {
        serverPanic("desHash()");
    }

    o->lru = lru;
    return o;
}

/* reference rdb.c rdbSaveObject() */
sds serZset(sds dst, robj *o) {
    serverAssert(o->type == OBJ_ZSET);

    if (o->encoding == OBJ_ENCODING_ZIPLIST) {
        size_t zip_list_bytes_len = ziplistBlobLen(o->ptr);
        dst = sdscatlen(dst, &zip_list_bytes_len, sizeof(size_t));
        dst = sdscatlen(dst, o->ptr, zip_list_bytes_len);

    } else if (o->encoding == OBJ_ENCODING_SKIPLIST) {
        zset *zs = o->ptr;
        zskiplist *zsl = zs->zsl;
        uint64_t zsl_length = zsl->length;
        dst = sdscatlen(dst, &zsl_length, sizeof(uint64_t));
        zskiplistNode *zn = zsl->tail;
        while (zn != NULL) {
            sds ele = zn->ele;
            size_t ele_len = sdslen(ele);
            dst = sdscatlen(dst, &ele_len, sizeof(size_t));
            dst = sdscatlen(dst, ele, ele_len);
            double score = zn->score;
            dst = sdscatlen(dst, &score, sizeof(double));

            zn = zn->backward;
        }
    } else {
        serverPanic("serZset()");
    }

    return dst; 
}

robj *desZset(char *s, size_t len, uint32_t lru) {
    serverAssert(len >= 1+sizeof(uint32_t));
    int zset_rock_type = *s;
    s += 1+sizeof(uint32_t);
    len -= 1+sizeof(uint32_t);

    robj *o = NULL;
    if (zset_rock_type == ROCK_TYPE_ZSET_ZIPLIST) {
        size_t zip_list_bytes_len = *((size_t*)s);
        s += sizeof(size_t);
        len -= sizeof(size_t);
        serverAssert(len == zip_list_bytes_len);

        char *ziplist = zmalloc(zip_list_bytes_len);
        memcpy(ziplist, s, zip_list_bytes_len);

        o = createObject(OBJ_ZSET, ziplist);
        o->encoding = OBJ_ENCODING_ZIPLIST;

    } else if (zset_rock_type == ROCK_TYPE_ZSET_SKIPLIST) {        
        serverAssert(len >= sizeof(uint64_t));
        uint64_t zset_len = *((uint64_t*)s);
        s += sizeof(uint64_t);
        len -= sizeof(uint64_t);
        
        o = createZsetObject();
        zset *zs = o->ptr;

        if (zset_len > DICT_HT_INITIAL_SIZE)
            dictExpand(zs->dict, zset_len);
        
        while (zset_len--) {
            sds sdsele;
            double score;
            zskiplistNode *znode;

            serverAssert(len >= sizeof(size_t));
            size_t ele_len = *((size_t*)s);
            s += sizeof(size_t);
            len -= sizeof(size_t);
            serverAssert(len >= ele_len);
            sdsele = sdsnewlen(s, ele_len);
            s += ele_len;
            len -= ele_len;

            serverAssert(len >= sizeof(double));
            score = *((double*)s);
            s += sizeof(double);
            len -= sizeof(double);

            znode = zslInsert(zs->zsl, score, sdsele);
            dictAdd(zs->dict, sdsele, &znode->score);
        }
        serverAssert(len == 0);        
    } else {
        serverPanic("desZset()");
    }

    o->lru = lru;
    return o;
}


/* reference rdbSaveObject() */
sds serObject(robj *o) {
    sds dst;
    dst = serObjectType(o);     // first byte is the object type

    // then 24bits for LRU/LFU
    uint32_t lru = o->lru;
    dst = sdscatlen(dst, &lru, sizeof(uint32_t));

    if (o->type == OBJ_STRING) {
        dst = serString(dst, o);
    } else if (o->type == OBJ_LIST) {
        dst = serList(dst, o);
    } else if (o->type == OBJ_SET) {
        dst = serSet(dst, o);
    } else if (o->type == OBJ_HASH) {
        dst = serHash(dst, o);
    } else if (o->type == OBJ_ZSET) {
        dst = serZset(dst, o);
    }else {
        serverPanic("Unknown object type");
    }

    return dst;
}

/* descerilize to object, buf, len from Rocksdb */
robj *desObject(void *buf, size_t len) {
    serverAssert(len > 0);
    char rock_type = *(char*)buf;

    serverAssert(len >= 1+sizeof(uint32_t));
    /* then 32bit -> 24bit LRU/LFU */
    uint32_t lru = *((uint32_t*)((char*)buf+1));

    switch (rock_type) {
    case ROCK_TYPE_STRING:
        return desString(buf, len, lru);
    case ROCK_TYPE_LIST_QUICKLIST:
        return desList(buf, len, lru);
    case ROCK_TYPE_SET_HT:
    case ROCK_TYPE_SET_INTSET:
        return desSet(buf, len, lru);
    case ROCK_TYPE_HASH_HT:
    case ROCK_TYPE_HASH_ZIPLIST:
        return desHash(buf, len, lru);
    case ROCK_TYPE_ZSET_ZIPLIST:
    case ROCK_TYPE_ZSET_SKIPLIST:
        return desZset(buf, len, lru);
    default:
        serverPanic("desObject type error!");
        return NULL;        
    }
}

void _test_print_ziplist(unsigned char *zl) {
    unsigned char *p = ziplistIndex(zl,0);
    unsigned char *vstr;
    unsigned int vlen;
    long long vll;

    int i = 0;
    while(p) {
        ziplistGet(p,&vstr,&vlen,&vll);

        if (vstr) 
            serverLog(LL_NOTICE, "ql index = %d, string, len = %u, str = %s", i, vlen, vstr);
        else
            serverLog(LL_NOTICE, "ql index = %d, long long ,long val = %lld", i, vll);

        p = ziplistNext(zl,p);
        ++i;
    }
}

void _test_print_ht(dict *h) {
    dictIterator *dit = dictGetIterator(h);
    dictEntry *de;

    int no = 0;
    while ((de = dictNext(dit))) {
        sds field = dictGetKey(de);
        sds val = dictGetVal(de);

        serverLog(LL_NOTICE, "no = %d, field = %s, val = %s", no, field, val);
        ++no;
    }

    dictReleaseIterator(dit);
}

void _test_print_skiplist(zset *zs) {
    dictIterator *di = dictGetIterator(zs->dict);
    dictEntry *de;
    int i = 0;
    while ((de = dictNext(di))) {
        sds key = dictGetKey(de);
        double *score = dictGetVal(de);
        serverLog(LL_DEBUG, "zset dict i = %d, key = %s, score = %lf", i, key, *score);
        ++i;
    }
    dictReleaseIterator(di);

    serverAssert(zs->zsl->length == dictSize(zs->dict));

    zskiplist *zsl = zs->zsl;
    zskiplistNode *zn = zsl->tail;
    i = 0;
    while (zn) {
        serverLog(LL_NOTICE, "zset skiplist i = %d, key = %s, score = %lf", i, zn->ele, zn->score);
        zn = zn->backward;
        ++i;
    }
}

void _test_ser_des_zset_ziplist() {
    char lookup[] = "abc";
    sds key = sdsnewlen(lookup, sizeof(lookup)-1);
    dictEntry *de = dictFind(server.db[0].dict, key);
    if (!de) {
        serverLog(LL_NOTICE, "de is null for key = %s", lookup);
        return;
    } 
    robj *val = dictGetVal(de);    
    if (val->type != OBJ_ZSET || val->encoding != OBJ_ENCODING_ZIPLIST) {
        serverLog(LL_NOTICE, "val type or encoding not correct! type = %u", val->type);
        return;
    }
    sds ser = serObject(val);
    robj *des = desObject(ser, sdslen(ser));

    serverLog(LL_NOTICE, "des encoding = %s", des->encoding == OBJ_ENCODING_ZIPLIST ? "ziplist" : "not ziplist!!!");
    if (des->encoding == OBJ_ENCODING_ZIPLIST)
        _test_print_ziplist(des->ptr);
}

void _test_ser_des_zset_skiplist() {
    char lookup[] = "abc";
    sds key = sdsnewlen(lookup, sizeof(lookup)-1);
    dictEntry *de = dictFind(server.db[0].dict, key);
    if (!de) {
        serverLog(LL_NOTICE, "de is null for key = %s", lookup);
        return;
    } 
    robj *val = dictGetVal(de);    
    if (val->type != OBJ_ZSET || val->encoding != OBJ_ENCODING_SKIPLIST) {
        serverLog(LL_NOTICE, "val type or encoding not correct! type = %u, encoding = %u", val->type, val->encoding);
        return;
    }
    sds ser = serObject(val);
    robj *des = desObject(ser, sdslen(ser));

    serverLog(LL_NOTICE, "des encoding = %s", des->encoding == OBJ_ENCODING_SKIPLIST ? "skiplist" : "not skiplist!!!");
    if (des->encoding == OBJ_ENCODING_SKIPLIST)
        _test_print_skiplist(des->ptr);
}

void _test_ser_des_zset() {
    // _test_ser_des_zset_ziplist();
    _test_ser_des_zset_skiplist();
}

void _test_ser_des_hash_ziplist() {
    char lookup[] = "abc";
    sds key = sdsnewlen(lookup, sizeof(lookup)-1);
    dictEntry *de = dictFind(server.db[0].dict, key);
    if (!de) {
        serverLog(LL_NOTICE, "de is null for key = %s", lookup);
        return;
    } 
    robj *val = dictGetVal(de);    
    if (val->type != OBJ_HASH || val->encoding != OBJ_ENCODING_ZIPLIST) {
        serverLog(LL_NOTICE, "val type or encoding not correct! type = %u", val->type);
        return;
    }
    sds ser = serObject(val);
    robj *des = desObject(ser, sdslen(ser));

    serverLog(LL_NOTICE, "des encoding = %s", des->encoding == OBJ_ENCODING_ZIPLIST ? "ziplist" : "not ziplist!!!");
    if (des->encoding == OBJ_ENCODING_ZIPLIST)
        _test_print_ziplist(des->ptr);
}

void _test_ser_des_hash_ht() {
    char lookup[] = "abc";
    sds key = sdsnewlen(lookup, sizeof(lookup)-1);
    dictEntry *de = dictFind(server.db[0].dict, key);
    if (!de) {
        serverLog(LL_NOTICE, "de is null for key = %s", lookup);
        return;
    } 
    robj *val = dictGetVal(de);    
    if (val->type != OBJ_HASH || val->encoding != OBJ_ENCODING_HT) {
        serverLog(LL_NOTICE, "val type or encoding not correct! type = %u", val->type);
        return;
    }
    sds ser = serObject(val);
    robj *des = desObject(ser, sdslen(ser));

    serverLog(LL_NOTICE, "des encoding = %s", des->encoding == OBJ_ENCODING_HT ? "ht" : "not ht!!!");
    if (des->encoding == OBJ_ENCODING_HT)
        _test_print_ht(des->ptr);
}

void _test_ser_des_hash() {
    // _test_ser_des_hash_ziplist();
    _test_ser_des_hash_ht();
}

void _test_ser_des_set_intset(void) {
    char lookup[] = "abc";
    sds key = sdsnewlen(lookup, sizeof(lookup)-1);
    dictEntry *de = dictFind(server.db[0].dict, key);
    if (!de) {
        serverLog(LL_NOTICE, "de is null for key = %s", lookup);
        return;
    } 
    robj *val = dictGetVal(de);    
    if (val->type != OBJ_SET || val->encoding != OBJ_ENCODING_INTSET) {
        serverLog(LL_NOTICE, "val type or encoding not correct! type = %u", val->type);
        return;
    }
    sds ser = serObject(val);
    robj *des = desObject(ser, sdslen(ser));
    intset *is = des->ptr;
    serverLog(LL_NOTICE, "is->encoding = %u, is->lenght = %u", is->encoding, is->length);
    for (int i=0; i < (int)(is->encoding*is->length); ++i)
        serverLog(LL_NOTICE, "i = %d, byte = %d", i, (int)is->contents[i]);
}

void _test_ser_des_set_ht(void) {
    char lookup2[] = "def";
    sds key2 = sdsnewlen(lookup2, sizeof(lookup2)-1);
    dictEntry *de2 = dictFind(server.db[0].dict, key2);
    if (!de2) {
        serverLog(LL_NOTICE, "de2 is null for key = %s", lookup2);
        return;
    }
    robj *val2 = dictGetVal(de2);
    if (val2->type != OBJ_SET || val2->encoding != OBJ_ENCODING_HT) {
        serverLog(LL_NOTICE, "val2 type or encoding not correct! type = %u", val2->type);
        return;
    }

    sds ser2 = serObject(val2);

    robj *des2 = desObject(ser2, sdslen(ser2));
    dictIterator *di = dictGetIterator(des2->ptr);
    dictEntry *de;
    while ((de = dictNext(di))) {
        sds key = dictGetKey(de);
        sds val = dictGetVal(de);

        serverLog(LL_NOTICE, "set ht, key = %s, val is %s", key, val == NULL ? "null" : "not null");
    }
    dictReleaseIterator(di);
}

void _test_ser_des_set(void) {
    // try use redis client to insert integer to key abc which type is set
    // _test_ser_des_set_intset();
    _test_ser_des_set_ht();
}

void _test_print_quicklist(quicklist *ql) {
    serverLog(LL_NOTICE, "ql node count = %lu, entry count = %lu", ql->len, ql->count);
    quicklistIter *qit = quicklistGetIterator(ql, AL_START_HEAD);
    quicklistEntry entry;
    int index = 0;
    while (quicklistNext(qit, &entry)) {
        if (entry.value) {
            sds print = sdsnewlen(entry.value, entry.sz);
            serverLog(LL_NOTICE, "index = %d, entry sz = %u, entry val = %s", index, entry.sz, print);
        } else {
            serverLog(LL_NOTICE, "index = %d, entry long value = %lld", index, entry.longval);
        }
        ++index;
    }
    quicklistReleaseIterator(qit);
}

void _test_ser_des_list(void) {
    serverLog(LL_NOTICE, "_test_ser_des_list");

    /* try use redis client to lpush some value to key abc */
    char lookup[] = "abc";
    sds key = sdsnewlen(lookup, sizeof(lookup)-1);
    dictEntry *de = dictFind(server.db[0].dict, key);
    if (!de) {
        serverLog(LL_NOTICE, "de is null for key = %s", lookup);
        return;
    } 
    robj *val = dictGetVal(de);
    if (val->type != OBJ_LIST || val->encoding != OBJ_ENCODING_QUICKLIST) {
        serverLog(LL_NOTICE, "val type or encoding not correct!");
        return;
    }
    quicklist *ql = val->ptr;
    _test_print_quicklist(ql);

    robj *list = createQuicklistObject();
    quicklistSetOptions(list->ptr, server.list_max_ziplist_size,
                        server.list_compress_depth);

    sds v1 = sdsnewlen("xxx", 3);
    long long l = -1234567;
    sds v2 = sdsfromlonglong(l);
    quicklistPushTail(list->ptr, v1, sdslen(v1));
    quicklistPushTail(list->ptr, v2, sdslen(v2));
    sdsfree(v1);
    sdsfree(v2);
    _test_print_quicklist(list->ptr);

    sds serialize = serObject(list);
    robj *desObj = desObject(serialize, sdslen(serialize));
    _test_print_quicklist(desObj->ptr);
}

void _test_ser_des_string(void) {
    serverLog(LL_NOTICE, "_test_ser_des_string");

    robj *s1 = createStringObjectFromLongLongForValue(134123);
    sds ser1 = serObject(s1);
    size_t len1 = sdslen(ser1);
    void *p1 = zmalloc(len1);
    memcpy(p1, ser1, len1);
    robj *d1 = desString(p1, len1, s1->lru);
    serverAssert(d1->refcount == 1);

    if (s1->type != d1->type) 
        serverLog(LL_NOTICE, "1 type!");
    else if (s1->encoding != d1->encoding)
        serverLog(LL_NOTICE, "1 encoding!");
    else if ((long long)(s1->ptr) != (long long)(d1->ptr))
        serverLog(LL_NOTICE, "1 long val, sval = %lld, dval = %lld", (long long)(s1->ptr), (long long)(d1->ptr));

    robj *s2 = createEmbeddedStringObject("abc", 3);
    sds ser2 = serObject(s2);
    size_t len2 = sdslen(ser2);
    void *p2 = zmalloc(len2);
    memcpy(p2, ser2, len2);
    robj *d2 = desString(p2, len2, s2->lru);
    serverAssert(d2->refcount == 1);

    if (s2->type != d2->type) 
        serverLog(LL_NOTICE, "2 type!");
    else if (s2->encoding != d2->encoding)
        serverLog(LL_NOTICE, "2 encoding!");
    else if (sdslen(s2->ptr) != sdslen(d2->ptr))
        serverLog(LL_NOTICE, "2 sds len!");
    else if (memcmp(s2->ptr, d2->ptr, len2-2))
        serverLog(LL_NOTICE, "2 memcmp!");

    robj *s3 = createRawStringObject("aadfcrghsdgggggggggggadbAFWEdsar4dadsrd423FASFASXASDFASR3ADFASDFASFASR34RFADSFSADFSAFXEEdsdec", 60);
    sds ser3 = serObject(s3);
    size_t len3 = sdslen(ser3);
    serverLog(LL_NOTICE, "len3 = %lu", len3);
    void *p3 = zmalloc(len3);
    memcpy(p3, ser3, len3);
    robj *d3 = desString(p3, len3, s3->lru);
    serverAssert(d3->refcount == 1);

    if (s3->type != d3->type) 
        serverLog(LL_NOTICE, "3 type!");
    else if (s3->encoding != d3->encoding)
        serverLog(LL_NOTICE, "3 encoding!");
    else if (sdslen(s3->ptr) != sdslen(d3->ptr))
        serverLog(LL_NOTICE, "3 sds len!");
    else if (memcmp(s3->ptr, d3->ptr, len3-2)) {
        serverLog(LL_NOTICE, "3 memcmp!");
        serverLog(LL_NOTICE, "3 s3 = %s", s3->ptr);
        serverLog(LL_NOTICE, "3 d3 = %s", d3->ptr);
        for (int i=0; i < (int)len3; ++i) {
            char c1 = *((char*)(s3->ptr)+i);
            char c2 = *((char*)(d3->ptr)+i);
            if (c1 != c2) {
                serverLog(LL_NOTICE, "i = %d, c1 = %c, c2 = %c", i, c1, c2);
            }
        }
    }

    decrRefCount(s1);
    decrRefCount(d1);
    zfree(p1);
    decrRefCount(s2);
    decrRefCount(d2);
    zfree(p2);
    decrRefCount(s3);
    decrRefCount(d3);
    zfree(p3);
}
