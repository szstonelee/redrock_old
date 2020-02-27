[Back Top Menu](../README.md)

# Backup and Persistence

## DO NOT use the rocksdb folder

First and most, do not use the Rocksdb folder for backup. [About the Rocksdb folder](howrun_cn.md). Becasue the values in memory do not be dumped to disk. You will lose your data if you use Rocksdb folder.

You can use two ways to backup, RDB or AOF, like the traditional Redis backup ways, check more about how Redis backup data using RDB or AOF, [click here to Redis website]](https://redis.io/topics/persistence)

## Huge Consumed Time and Disk Usage

Because we use RedRock for such user case, major data are in disk while all keys and minor value in memory. We call the keys with value in memory 'HotKey'.

RedRock's backup is not like what Redis do. For traditional Redis's backup, backup is scan all the key/value in memory and take some times to write them to disk. But for RedRock's backup, there are huge disk access. And because at this time [maxmemory](howrun_cn.md) has already been reached, RedRock has to dump some HotKey to disk for more room of memory. So a lot of previous HotKey will be cold for the backup. You need to consider three cost carefully:
1. the cost of the time for disk access
2. the cost of the disk storage
3. the cost of HotKey
I suggest you plan your strategy of backup and test it.

NOTE: 
1. For maxmemory, it is not taken account for the memory which is used by the fork() method. 
2. Disk access is much slower than memory access. Usualy 10,000 times slow.
3. AOF is much better than RDB. Because RDB is always full scan for all keys. But AOF is seldomly scan all keys but the first time full scan for AOF can not be avoidable.

## Snapshot and Consistence

Backup snapshot is created at the accurate time you issue the backup command. The backup data is a snapshot which means it won't be affected by the write commands afterwards even the time for backup is VERY long. When you use the fork() way to backup, RedRock can service all clients during the period of backup with small impacts. All clients can write(delete)/read any data freely. Backup and servinng clients are isolated when backup use fork() way.