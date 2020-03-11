[Back Top Menu](../README.md)

# Config and Run

Redis can simiplly run with no arguments. In CLI, type ./redis-server and run redis。

But if you want the RedRock new features, you need the following **FIVE** config parameters.

NOTE: there a lot of place needs sudo permission. I suggest you grant yourself sudo permission, like 
```
su <your account>
```

## Config Parameters for RedRock
### 1. maxmemory
First and most, you need set "maxmemory" bigger than zero, e.g. 100mb or 100m

Because if no limit for memory, we do not need RedRock and its disk feature.

It means if maxmemory == 0, we disable RedRock features completely. 

NOTE:
+ "maxmemory" needs to be big enough to hold all keys. For example, assuming key average size is 100 bytes and you want 100 Million keys, you need setup "maxmemroy" at least 10G. Otherwise, you check the folllowing 'maxmemory-only-for-rocksdb' details
+ "maxmemory" can not be too small, e.g. a couple of megabytes, because space is too small for RedRock to rock. Suggest at least 100M for "maxmemory"
+ Do not setup "maxmemory" as much as your real machine/VM memory size because OS needs memory too. Redis do not count tempary buffer like backup for "maxmemory". You can reference：https://redis.io/topics/memory-optimization
+ When 'enable-rocksdb-feature' is "yes", if you modify maxmemory to non-zero, by a congfig-file or online changing config command of "config set", you can not change it back to zero

### 2. enable-rocksdb-feature
When maxmemory != 0, and enable-rocksdb-feature == yes, it means RedRock work, i.e. rock!

The default value for enable-rocksdb-feature is "no". You need set it to "yes" explicitly.

### 3. rockdbdir
It is for which folder where RedRock store data in disk using Rocksdb engine.

#### default value

It is optional and the default value for rockdbdir is "./redrock_rocksdb/".

If you compile in ‘src’, redrock_rocksdb is in the 'src'

NOTE:
+ The folder name must include '/' as the last character 
+ The rockdbdir folder is only for temporary usage. Every time when RedRock starts, it deletes the sub folders in it.
+ After redis-server stop, you can not use the rockdbdir folder as a backup because it does not include the key/value in memory. [check backup for RedRock for more details](persistence_en.md)
+ If you want a real backup, please use RDB/AOF. Reference：[Backup and Persistence](persistence_en.md)

#### Linux folder permission

You need set folder permission for you specific 'rockdbdir', like /opt/redrock_rocksdb/

If you do not have the permission for the folder, you will see such error message when start

```
rockapi write status = IO error: while open a file for lock: /opt/redrock_rocksdb/0/LOCK: Permission denied
```

to solve this problem, you have three choices

1. Use other folder which you have permission and set rockdbdir when RedRock starts
2. sudo to start your RedRock, like
```
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes
```
3. chmod your folder permission, like 
```
sudo chmod -R 777 /opt/redrock_rocksdb
```

### 4. maxmemory-only-for-rocksdb
This parameter is optional. The default value is "yes".

#### when maxmemory-only-for-rocksdb == yes
+ When memory usage is close to "maxmemory"，RedRock will try to dump value to disk but always keep all keys in memory.
+ If the above situation goes on worse, that is, almost every value goes to disk and the memory usage will still reach "maxmemory" limit, RedRock will deny those commands which will consume memory (NOTE: not by a judgement from real increment memory because it is too complicated). Usually these kinds of command are for updating/inserting. So in this situation, RedRock will be something like Read-Only server (but you can go on with delete).
+ If you delete some keys in the above situation and make new rooms for memory, RedRock recover and go on for all service as usual.

#### when maxmemory-only-for-rocksdb == no
As usual, RedRock will try to dump every value to storage to save room for memory until it comes to the above situation where almost every value goes to disk and the memory is still not enough. 

It will come to two solutions
+ If Redis is configured as being able to evict keys, i.e. deleting keys, Redis will evict some keys for memory room.
Please reference：https://redis.io/topics/lru-cache
+ If Redis is configured to not evict keys, Redis will deny some kinds of commands. In such way, Redis is like Read-Only server but can go on with delete commands to make room for memory.

#### 5. max-hope-hot-keys
'max-hope-hot-keys' is only effective when maxmemory-only-for-rocksdb == no.

It is optional with default value 1000.

When memory is coming to the limit of memory and RedRock will evict the keys, RedRock will try its best to keep 'max-hope-hot-keys' keys with value in memory. The hot keys with value in memory are chosen from LRU/LFU algorithm when you config Redis. [Check more detail about eviction algorithm Redis support](https://redis.io/topics/lru-cache)

It is complicated, I will give your an example:

Suppose you config RedReck maxmemory to 100M and you start to insert 3 million keys with its value to RedRock. Key size is small, just tens of bytes, but value's size is much bigger, let it say, a couple of kilo bytes.

When maxmemory-only-for-rocksdb == yes, RedRock will try its best to insert all keys with its value in memory without eviction. But RedRock can not do it for only 100M memory. RedRock will dump some value to disk to insert the coming-insert keys. It will come to this point, 100M memory is used up, and all the values of the previous inserted-keys have been dumped to disk. In this situation, RedRock will deny any more insert commands.

So what if we change maxmemory-only-for-rocksdb to 'no'. This time, for inserting more keys, RedRock can evict some previous keys for new room for incoming-insert keys. But RedRock will start to do it only after all previous keys' values dumped to disk. And the eviction policy has nothing about LRU/LFU but using random algarithm (because all values in disk!!!).

This is what 'max-hope-hot-keys' is for. When setting it to a non-zero number, we tell RedRock to keep at most 'max-hope-hot-keys' keys with value in memory. And these keys are chosen by LRU/LFU algorithm. Let it say, max-hope-hot-keys == 1000, it means, when inserting 3 million keys in one client, during the time, other clients visit some keys like get command, when 3 million keys are inserted, about 1 million keys are evicted because we grant RedRock to evict, but almost the frequently-visited 1000 keys are saved.

### Other Redis Config Parameters
Please reference https://redis.io/topics/lru-cache

When we select some keys to delete or dump in the above situation, there are some policies we can use
+ LRU: keep the recent visited keys to survive
+ LFU: keep the most frequent visited keys to survive  
+ RANDOM: every key has the born-fair-oppertunity to survive or be killed

## How to config

How to make the above config parameters to be effective, there are two ways.

### Command Line

e.g. 1: 100M Max Memory and No Eviction
```
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes
```
e.g. 2: 100M Max Memory and Evicition Randomly
```
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --maxmemory-policy allkeys-lfu
```
e.g. 3: 100M Max Memory and Eviction with LFU of at least 10K Keys
```
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --maxmemory-policy allkeys-lfu --max-hope-hot-keys 10000
```


### Config file

Please modify redis.conf, add enable-rocksdb-feature, rockdbdir, maxmemory-only-for-rocksdb, max-hope-hot-keys. 
```
./redis-server redis.conf
```
Please reference：https://redis.io/topics/config

### Can not change online

You can not use "config set" commands to set these parameters online except 'maxmemory' and 'max-hope-hot-keys'. 

### Check Effictiveness

Use reids-cli to connect the redis-server, then
```
config get maxmemory
config get enable-rocksdb-feature
config get rockdbdir
config get maxmemory-only-for-rocksdb
config get max-hope-hot-keys
```

## Other related questions

There are some questions we can think about.

+ If not set maxmemory for Redis, and continously consume memory, what happened for Redis?

Redis will use OS page swap files as memory. In this way, memory is unlimited for OS, but it is really really slow.
Please reference：https://redis.io/topics/faq#what-happens-if-redis-runs-out-of-memory

+ In the above situation, can we use SSD as OS swap file because SSD is pretty faster than HDD?

The answer is simiplly **NO**. Please reference an article from Redis author antierz. [Redis with an SSD swap, not what you want](http://antirez.com/news/52)

+ If we enable maxmemory, but do not enable enable-rocksdb-feature, what will happen? 

In this way, RedRock features are disabled. It is a traditional Redis server. You can reference: https://redis.io/topics/lru-cache