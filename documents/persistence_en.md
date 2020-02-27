[Back Top Menu](../README.md)

# Backup and Persistence

## DO NOT use the rocksdb folder

First and most, do not use the Rocksdb folder for backup. [About the Rocksdb folder](howrun_cn.md). Becasue the values in memory do not be dumped to disk. You will lose your data if you use Rocksdb folder.

You can use two ways to backup, RDB or AOF, like the traditional Redis backup ways, check more about how Redis backup data using RDB or AOF, [click here to Redis website for more details about RDB/AOF](https://redis.io/topics/persistence)

## Huge Consumed Time and Disk Usage

We use RedRock for such user case - major data are in disk while all keys and minor values in memory. We call the keys with value in memory 'HotKeys'.

RedRock's backup is not like what Redis do. For traditional Redis's backup, backup is a scan for all the key/value in memory and takes some times to write them to disk. But for RedRock's backup, there are huge disk access because the major part of the dataset is in the disk. Even worse, at this time [maxmemory](howrun_cn.md) has already been reached, RedRock has to dump some HotKeys to disk for more room of memory for the backup. So a lot of HotKeys will become cold. 

You need to consider three costs very carefully:
1. the cost of the time for disk access
2. the cost of the disk storage
3. the cost of HotKeys to become cold after backup

I suggest you plan your strategy of backup and test it.

NOTE: 
1. For maxmemory, it is not taken account for the memory which is used by the fork() method. 
2. Disk access is much slower than memory access. Usualy 10,000 times slow.
3. AOF is much better than RDB. Because RDB is always a full scan for all keys. But AOF is seldomly a full scan for all keys and usually use incremental appending strategy. But the first time of full scan for AOF can not be avoidable.

## Snapshot and Consistence

Backup snapshot is created at the accurate time you issue the backup command. The backup data is a snapshot which means it won't be affected by the write commands afterwards even the time for backup is VERY long. When you use the fork() way to backup, RedRock can service all clients during the period of backup with small impacts. All clients can write(delete)/read any data freely. Backup and servinng clients are isolated when backup use fork() way.