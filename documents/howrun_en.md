[Back Top Menu](../README.md)

# Config and Run

Redis can simiplly run with no arguments. In CLI, type ./redis-server and run redis。

But if you want the RedRock new features, you need the following config parameters.

## Config Parameters for RedRock
### maxmemory
First and most, you need set "maxmemory" bigger than zero, e.g. 100mb.

Because if no limit for memory, we do not need RedRock and its backend storage.

It means if maxmemory==0, we disable RockRed features. 

NOTE:
1. "maxmemory" needs to be big enough to hold all keys. For example, assuming key average size is 100 bytes and you want 100 Million keys, you need setup "maxmemroy" at least 10G.
2. "maxmemory" can not be too small, e.g. a couple of megabytes, because the space is too small for RedRock to rock. Suggest at lease 100M for "maxmemory".
3. Do not setup "maxmemory" as much as your real machine/VM memory size because OS needs memory too. Redis do not count tempary buffer like backup for "maxmemory". You can reference：https://redis.io/topics/memory-optimization，

### enable-rocksdb-feature
When maxmemory != 0, and enable-rocksdb-feature == yes, it means RedRock work.

The default value for enable-rocksdb-feature is "no". You need set it to "yes" explicitly.

### rockdbdir
It is for which folder where RedRock store data in disk using Rocksdb engine.

The default value for rockdbdir is "/opt/redrock". 

NOTE: 
1. The rockdbdir folder is only for temporary usage. Every time when RedRock starts, it deletes the folder.
2. After redis-server stop, you can not use the rockdbdir folder as a backup because it does not include the key/value in memory.
3. If you want a real backup, please use RDB/AOF. Reference：[Backup and Persistence](persistence_en.md)

### maxmemory-only-for-rocksdb
This parameter is optional. The default value is "yes".

#### when maxmemory-only-for-rocksdb == yes
1. When memory usage is close to "maxmemory"，RedRock will try to dump value to storage but always keep key in memory.
2. If the above situation go on worse, that is, almost every value goes to storage and the memory usage will still reach "maxmemory" limit, RedRock will deny those commands which will consume memory. Usually these kinds of command are for updating/inserting. So in this situation, RedRock will be something like Read-Only server.
3. If you delete some keys in the above situation and make new rooms for memory, RedRock recover and go on for all service as usual.

#### when maxmemory-only-for-rocksdb == no
As usual, RedRock will try to dump every value to storage to save room for memory until it comes to the above situation where almost every value goes to storage and the memory is still not enough. 

It will come to two solutions
1. If Redis is configured as to be able to delete keys, Redis will delete some key for memory room.
Please reference：https://redis.io/topics/lru-cache
2. If Redis is configured to not delete keys, Redis will deny some kinds commands. In such way, Redis is like Read-Only server.

### Other Redis Config Parameters
Please reference https://redis.io/topics/lru-cache

When we select some keys to delete or dump in the above situation, there are some policies we can use
1. LRU: keep the recent visited keys to survive
2. LFU: keep the most frequent visited keys to survive  
3. RANDOM: every key has the born-fair-oppertunity to survive or be killed

## How to config

How to make the above config parameters to be effective, there are two ways.

### Command Line

e.g.
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes
```
### Config file

Please modify redis.conf, add enable-rocksdb-feature, rockdbdir, maxmemory-only-for-rocksdb. 
Please reference：https://redis.io/topics/config

### Can not change online

You can not use "config set" commands to set these parameters online except maxmemory. 

### Check Effictiveness

Use reids-cli to connect the redis-server, then
```
config get maxmemory
config get enable-rocksdb-feature
config get rockdbdir
config get maxmemory-only-for-rocksdb
```

## Other related questions

There are some questions we can think about.

1. If not set maxmemory for Redis, and continously consume memory, what happened for Redis?

Redis will use OS page swap files as memory. In this way, memory is unlimited for OS, but it is really really slow.
Please reference：https://redis.io/topics/faq#what-happens-if-redis-runs-out-of-memory

2. In the above situation, can we use SSD as OS swap file because SSD is pretty faster than HDD?

The answer is simiplly NO. Please reference an article from Redis author antierz. [《Redis with an SSD swap, not what you want》](http://antirez.com/news/52)

3. If we enable maxmemory, but do not enable enable-rocksdb-feature, what will happen? 

In this way, RedRock features are disabled. It is a traditional Redis server. You can reference: https://redis.io/topics/lru-cache