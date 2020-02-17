
#ifndef __ROCK_H
#define __ROCK_H

#include "server.h"

/* Map object types to Rocksdb object types. Macros starting with OBJ_ are for
 * memory storage and may change. Instead Rocksdb types must be fixed because
 * we store them on disk. */
#define ROCK_TYPE_STRING 0
// #define ROCK_TYPE_LIST   1
#define ROCK_TYPE_SET_HT    2
// #define ROCK_TYPE_ZSET   3
#define ROCK_TYPE_HASH_HT   4
#define ROCK_TYPE_ZSET_SKIPLIST 5 /* ZSET version 2 with doubles stored in binary. */

/* Object types for encoded objects. */
// #define ROCK_TYPE_HASH_ZIPMAP    9
// #define ROCK_TYPE_LIST_ZIPLIST  10
#define ROCK_TYPE_SET_INTSET    11
#define ROCK_TYPE_ZSET_ZIPLIST  12
#define ROCK_TYPE_HASH_ZIPLIST  13
#define ROCK_TYPE_LIST_QUICKLIST 14

/* API */
void rock_print_debug();
void rock_debug_print_key_report();
void rock_test_resume_rock();
void rock_test_set_rock_key(char *keyStr);
void checkCallValueInRock(client *c);
void initRockPipe();
void initZeroRockJob();
void test_add_work_key(int dbid, char *key, size_t len);
void checkThenResumeRockClient(client *c);
void releaseRockKeyWhenFreeClient(client *c);
int dumpValueToRockIfNeeded();
void rockPoolAlloc(void);
size_t getMemoryOfRock();
robj* loadValFromRockForRdb(int dbid, sds key);

int init_rocksdb(int dbnum, char *path);
void teardown_rocksdb();

void rock_test_read_rockdb(char *key);
void rock_test_write_rockdb(char *val);
size_t getMemoryOfRock();
void initHotKeys();

void cmdCheckRockForOneKey(client *c, struct redisCommand *cmd, robj **argv, int argc, list *l);
void cmdCheckRockExcludeLastArg(client *c, struct redisCommand *cmd, robj **argv, int argc, list *l);
void cmdCheckRockForAllKeys(client *c, struct redisCommand *cmd, robj **argv, int argc, list *l);
void cmdCheckRockExcludeFirstArg(client *c, struct redisCommand *cmd, robj **argv, int argc, list *l);

#endif
