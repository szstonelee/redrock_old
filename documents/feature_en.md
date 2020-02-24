[Back Top Menu](../README.md)

# Supported Features

## All Redis Commmands

Yes. [You can click here for more reference](commands_en.md)

## All Redis Datastruce

One of reasons for Redis to be such successfull is it supports a lot of data structures.
It is not like Memcached to support only key/value pair.
Engineers are lazy to write serialized and descerialzed functions, aren't they?
The good thing is RedRock support these data structures like Redis.
* String
* Set
* Hast. It is good for memory efficiency, you can check Twitter engineer blog.
* List
* Sorted Set. Like priority queue, the BigO for it is Log(N), close to constant.
* pub/sub
* stream
* HyperLogLog
* Geo

## Support Backup
Yes。And it is a snapshot. No concern about consistency. [More details](persistence_en.md)

## Cache
Yes。Use Redis expire as before. As bonus, expire does not need reed disk. [Check here](commands_en.md)

## Support multi database
Yes。Default is the usual 16 database. You can config the number of database you want.
In [the Rocksdb folder](howrun_en.md), you can see the database id as sub-folder name.

## Max Memory Limit
Yes。RedRock can limit it in the space of max memory, just like Redis.
[But you need to know, when reaching the limit, what will happen?](howrun_en.md)

## LRU & LFU
Yes。[More details here](howrun_en.md)

## Support Replication, i.e., Master/Slave
Yes. Leader/Follower(i.e., Master/Slave) replica are supported.

## Sentinel
I think it would support when I check the codes.
But I have only one Mac, I can not test it. 
If you test it, please let me know the result. Thank you.

## Cluster
I think so from the codes.
But I have not tested it because I have only one machine.

## Distrubted Lock: RedLock
Yes, I think it should support.
When use RedLock, I suggest to check two different ideas on RedLock.
Martin Kleppmann: https://martin.kleppmann.com/2016/02/08/how-to-do-distributed-locking.html
antirez: http://antirez.com/news/101
I support Martin Kleppmann.

## Pipeline
Yes.
Pipeline is the key why Redis get the performance for Million rps in one machine.

## Transaction
Yes。[More details](commands_en.md)

## Blocking
Yes。[More details](commands_en.md)

## Subcribe/Publish
Yes。

## Stats
Yes。

## SlowLog
Yes。
But note, because reading disk is commonly take more time than executing only in memory.
The SlowLog's data is influenced by the reading/writing disk. I am sorry about that.

## Good for intensive writing to disk
Yes. Because it is LSM which Rocksdb use. 
From my Mac, I get the metric of 8 MB/s wrting. 
[Check more for Performance](performance_en.md)

## Key/Value length is same as Redis
Yes.
When you caome to SSDB, a similiar product, it has the limit of key length as it encoded the key. 
For Pita，I do not check the detail. But it is based on SSDB. So I doubt it.
[More details](peers_cn.md)
