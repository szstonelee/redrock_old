[中文帮助请点这里, click here for Chinese documents](documents/menu_cn.md) 

# RedRock Homepage

## Introduction
RedRock is a combination of [Redis](https://github.com/antirez/redis) and [Rocksdb](https://rocksdb.org/).

## Why
Redis is a wonderful NOSQL based on memory. But memory is too expensive. We hope,
* Same fast and strong as Redis
* Redis can extend to storage (HDD or SSD)

As SSD is becoming cheaper and has good performance, it is why I, enigneer Stone, code RedRock. 

I wish one stone to hit two birds. 

NOTE: 
I only code the project for a couple of months and only use my off-busy-work time. 
It is not as mature as a project for ten years. 
Wish you use it more and leave feedbacks at github. Thank you.

## RedRock Features
* Pure Redis. When not enable storage, RedRock is running almost the same codes as Redis
* Support all Redis commands
* Support expire, you can use it as Cache
* Support all Redis data structures, including, String,List,Set,Hash,SortedSet,Stream,HyperLogLog,Geo
* Support persistence, including RDB and AOF, backup could use fork() or main thread
* Can config maximum memory usage
* Policy for dumping value to storage supports LRU and LFU
* Support Leader/Follower(i.e. Master/Slave) replica
* I think it would support Sentinel
* I think it would support Cluster
* I think it would support distributed lock, but I think Martin Kleppmann is right 
* Support Redis Pipeline
* Support Redis Transaction
* Support Redis Blocking
* Support subscribe/publish
* Support original stats for Redis, plus our storage stats
* Support slow log
* Good for mass intensive writing to storage
* When main load is for hot keys, performance is almost the same as Pure Redis, i.e. Million rps for one node
* When coming to avarage random key visit pattern, performance maybe degrade by 1 maganitude order，i.e Hundreds K rps for one node
* Using Rocksdb library, but overload of memory in db engine is limited to tens MB
* Keep the main logic in main thread as Redis, low level for thread switch and race risks.
* Key/value length limit is same as Redis

# How Compile

[Based on C/C++, click for details](documents/compile_en.md)

# How Config and Run

[All config parameters of Redis, plus three added config parameter](documents/howrun_en.md)

# Supported Redis Commands

[Support all Redis commands, click for more notice](documents/commands_en.md)

# Supported Redis Feature

[Maybe support all Redis features, e.g. Master/Slave, Cluster, Transaction, more details.](documents/feature_en.md)

# Test Cases

[You can test it for these features.](documents/test_en.md)

# Performance

[I focus on the worst condition performance, more details.](documents/performance_en.md)

# Backup and Persistence

[How backup and persist the memory data with the storage data.](documents/persistence_en.md)

# Peers Similar Projects

[You can compare and choose other project from the similiar peers.](documents/peers_en.md)