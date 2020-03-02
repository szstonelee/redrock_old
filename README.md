[中文帮助请点这里, click here for Chinese documents](documents/menu_cn.md) 

# RedRock Homepage

## Introduction
RedRock is a combination of [Redis](https://github.com/antirez/redis) and [Rocksdb](https://rocksdb.org/).

## Why
Redis is a wonderful NOSQL based on memory. But memory costs more than disk. We hope something,
* Same fast and strong as Redis
* Extend to disk (HDD or SSD)

As SSD is becoming cheaper and has good performance, it is why I, enigneer Stone, code RedRock. 

I wish one stone to hit two birds. 

NOTE: 
I only code the project for a couple of months and only use my off-busy-work time. 
It is not as mature as a project for ten years. 
Wish you use it more and leave feedbacks at Github. Thank you.

## RedRock Features
* Pure Redis. When no enable disk, RedRock is running almost the same codes as Redis
* Use Redis protocol, so no need to adjust one line of your client codes or config files
* Keep all keys in memory but only hot key's value in memory with most other values in disk
* Your dataset volume can be one hundred times of your memory
* Support all Redis commands except module's commands
* Support expire and eviction, you can use it as Cache
* Support all Redis data structures, including String,List,Set,Hash,SortedSet,Stream,HyperLogLog,Geo
* Support persistence, including RDB and AOF, in fork() child process or main thread
* Can config maximum memory
* Policy for dumping value to disk supports LRU and LFU
* If use eviction, you can specify LRU or LFU, and the max keys with value in memory
* Support Replication: Leader/Follower(i.e. Master/Slave) replica
* I think it would support Redis Sentinel
* I think it would support Reids Cluster. So no need for Twemproxy and Codis
* I think it would support distributed lock, RedLock, but I think Martin Kleppmann is right 
* Support Redis Pipeline
* Support Redis Transaction, including WATCH command
* Support Redis Blocking
* Support Script(LUA)
* Support Redis Subscribe/Publish
* Support Redis Stream
* Support original stats for Redis including SlowLog, plus RedRock specific stats
* Good for mass intensive writing to disk. For regular SSD, you can write new data at speed 20M/s
* Data is compressed in disk, you only need disk volume from 10% to half of your dataset
* When main load is for hot keys, performance is almost the same as Pure Redis, i.e. Million rps for one node
* When coming to avarage random key visit pattern, performance maybe degrade by 1 maganitude order，I guess near Hundred Kilo rps for one node
* Using Rocksdb library, but overload of memory in db engine is limited to tens MB
* Keep the main logic in main thread as Redis, low level for thread switch and race risks.
* Key/value length limit is same as Redis
* Support multi database and can change the number

# How Compile

[Based on C/C++, click for details](documents/compile_en.md)

# How Config and Run

[All config parameters same as Redis, plus four added/optional RedRock config parameters](documents/howrun_en.md)

# Supported Redis Commands

[Support all Redis commands except module commands, click for more notice](documents/commands_en.md)

# Supported Redis Features

[Maybe support all Redis features, e.g. Master/Slave, Cluster, Transaction, LUA, more details.](documents/feature_en.md)

# Test Cases

[You can test RedRock for all features.](documents/test_en.md)

# Performance

[I focus on the worst condition performance, more details.](documents/performance_en.md)

# Backup and Persistence

[How backup and persist the whole dataset, memory data with most values in disk.](documents/persistence_en.md)

# Stats and Tools

[Some tools for stats](documents/stat_en.md)

# Peer Similar Projects

[You can compare and choose other projects from peers.](documents/peers_en.md)