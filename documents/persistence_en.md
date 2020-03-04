[Back Top Menu](../README.md)

# Backup and Persistence, Replication

If you want the dataset safe, we can use backup or replication. But for RedRock, it is very complicated when considering the memory limit with most values in disk. The following document is very tedius but I suggest you read it and think it over.

## DO NOT use the rocksdb folder

First and most, do not use the Rocksdb folder for backup. [About the Rocksdb folder](howrun_en.md). 

The values in memory do not be dumped to the rocksdb disk. You will lose your data if you use Rocksdb folder.

You can use three ways to secure your data. 

For backup, it is RDB or AOF, like the traditional Redis backup ways, check more about how Redis backup data using RDB or AOF, [click here to Redis website for more details about RDB/AOF](https://redis.io/topics/persistence). 

The last way is to use replication. [Click Redis replication documents](https://redis.io/topics/replication)

There are a lot of situations. You need to check the following configurations.

## Backup without RedRcok feature

When you start RedRock but [not enabling RedRock features](howrun_en.md), you can use two ways to backup.

First way is to fork() a child process. Second way is backuping in the main thread. It is traditional backup ways like Redis, [you can check Redis documents for more detail.](https://redis.io/topics/persistence).

The fork() way, when you use the commands of ['BGSAVE'](https://redis.io/commands/bgsave) or ['BGREWRITEAOF'](https://redis.io/commands/bgrewriteaof), is ASYNC way. Clients would not be annoyed with a background child process doing the backup job. And because Redis use COW, the memory overhead for the child process is not high.

The main thread way, when you use the command of ['SAVE'](https://redis.io/commands/save), is SYNC way. Every client will stop to response until the backup finish. Memory overload is not much. 

## Backup with RedRock feature

When you start RedRock with RedRock feature enabled. The two ways, SYNC or ASYNC, are the same as you use the above commands like Redis.

But the memory overload can be **VERY** different.

### when maxmemory-only-for-rocksdb == yes

If fork() a child process to backup, the memory of child process is not over the limit of [maxmemory](howrun_en.md). This means your total memory usage, i.e. your main process plus your child process, both are limited in a certain condition. But the tradeoff is that it will use huge time because of limited memory.

For backup in main thread, it is the same. Only one process without any child process, so only one [maxmemory](howrun_en.md) limit. The backup time is very long and no clients can be active until the backup finish.

### when maxmemory-only-for-rocksdb == no

This time, [maxmemory](howrun_en.md) has no effect. You can see a huge memory overhead in a child process or only in the main process. 

Be careful of this situation, because if 99% value is in disk, the memory usage could be 100 times when backup.

## Restore a backup when RedRock start with RedRock feature disabled

When RedRock start without RedRock feature, if a backup file, rdb or aof in the folder, it will load the backup file to memory first.

It is the same way as Redis. [You can check Redis document for more details.](https://redis.io/topics/persistence)

So be careful! If you backup a file where a lot of values coming from disk, the dataset in the backup file could be huge. When you start RedRock without RedRock feature, it will load the huge file to memory and eat up all your memory.

## Restore a backup when RedRock start with RedRock feature enabled

### when maxmemory-only-for-rocksdb == yes

When loading start, you can see a loading child process appear, and the memory for the loading child process is limited to the [maxmemory](howrun_en.md). It will take a long time to reload a backup file to memory but your memory is OK.

### when maxmemory-only-for-rocksdb == no

With the configure, no limit for memory when start loading a backup file. So your memory could be eaten up. But after the loading, when RedRecok start to service, the memory will shrink to the [maxmeory](howrun_en.md) by eviction (or deny service).

## Replication

### For the master

#### master start with RedRock feature disabled

With the config, RedRock is like traditional Redis. If the dataset can not be [Redis PSYNC](https://redis.io/commands/psync), it will create a child process for replication. It use a rdb file and COW, so the memory overhead is not high. Check [Redis Sync](https://redis.io/commands/sync) and [Redis Replication](https://redis.io/topics/replication)

But because we do not enable RedRock feature, the dataset is small to be fit in the memory.

#### master start with RedRock feature enabled

If PSYNC ok, it will use psync. But because we use RedRock with RedRock feature enabled, it means the dataset is huge, and usually PSYNC does not work.

So it comes to SYNC way. RedRock will create a child process to save a rdb file. The memory overhad for the child process will be limited to the maxmemory limit. The good news is that memory is OK for child process. But there is some bad news. 

First, the backup rdb file for replication will take a long time because the limited memory.

Second, during the time, master need to accumulate the new commands coming from its clients and it will use a buffer for the accumulation. The buffer may be large if the backup time is long (I am sure it will be very long) and the new comming commands are too many. And the buffer size is not accounted for the maxmemory because it is temporary. 

### For the slave

#### slave start with RedRock feature disabled

The slave has no memory limit for replication even you set maxmemory. So it could eat up your memory if the dataset in master is huge.

Be careful of the situation.

#### slave start with RedRock feature enabled

The slave will limit the memory to maxmemory. The memory is OK but it will take a long time for replication.

### Always in PSYNC perfect situation

If differntial data for replication is always small, i.e. master/slave are in PSYNC mode, we do not worry about the memory and the time. We can both use master/slave with RedRock feature enabled. 

So keeping the connection between master/slave not broken is very important for this situation. Or the broken time is very short for master/slave as no need for a sync replication.

And master/slave need to start when dataset is not huge.

## Conclusion

I suggest you plan your strategy of backup or replication and test it. 

The situations are very complicated because RedRock need to deal with both the memory and the disk.

## Snapshot and Consistence

Backup snapshot is created at the accurate time you issue the backup command. The backup data is a snapshot which means it won't be affected by the write commands afterwards even the time for backup is VERY long. When you use the fork() way to backup, RedRock can service all clients during the period of backup with small impacts to clients. All clients can write(delete)/read any data freely. Backup and servinng clients are isolated when backup use fork() way.