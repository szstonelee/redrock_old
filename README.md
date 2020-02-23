[中文请点这里, click here for Chinese help](../menu_en.md) 

# Introduction

RedRock is a combination of Redis and Rocksdb.

Redis is a wonderful NOSQL for memory. But memory is too expensive. We hope:
* Something fast and strong as Redis
* Can support storage as backend

As SSD is becoming cheaper and has good performance, the project wants to have one stone to hit two birds. 

Features list of RedRock：
* Pure Redis. When no enable the storage, it is the same speed (i.e. almost the same codes).
* Support all Redis commands.
* Support all Redis data structures, including, String,List,Set,Hash,SortedSet,Stream,HyperLogLog
* Support persistence, including RDB and AOF, backup using fork() or no fork()
* Config the maximum memory usage.
* Support Leader/Follower(i.e. Master/Slave) replica.
* I think it supports Sentinel.
* I think it supports Cluster.
* Support Redis Pipeline.
* Support Redis Transaction.
* Support Redis Blocking.
* Support original stats from Redis, plus our storage stats.
* Support slow log.
* Good for mass intensive writing to storage.
* When main load is for hot keys, performance is the same as Pure Redis, i.e. Million RPQ for one node
* When come to avarage random key visits, performance may be degrate to 1 maganitude，i.e Hundreds RPQ for one node
* Using Rocksdb library, but it only consume tens MB.
* Keep the main logic in one thread as Redis, low cost for thread switch and lower the race risks.

# How Compile

[Based on C/C++, click for details](documents/compile_en.md)

# How Config and Run

[All config parameters of Redis, plus three added config parameter](documents/howrun_en.md)

# Supported Redis Commands

[Support all Redis commands, click for more notice](documents/commands_en.md)

# Supported Redis Feature

[Maybe support all Redis features, e.g. Master/Slave, Cluster, Transaction, more details](documents/feature_en.md)

# Test Case

[You can tested it for these feature](documents/test_en.md)

# Performance

[We focus on the worst condition performance, more details](documents/performance_en.md)

# Backup and Persistence

[How backup and persist the memory data with the storage data](documents/persistence_en.md)

# Peers Similar Projects

[You can compare and choose similiar projects from the peers](documents/peers_en.md)