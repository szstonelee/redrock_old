[Back Top Menu](../README.md)

# Supported Features

## Supported Platform

I tested RedRock on My MAC OS and Linux (which is running as VM in my Mac OS). 

Other platform like windows, I do not know. You can try.

## Redis Protocol

Yes.

So no source codes and config file needs to be modified for RedRock. 

Use it just like RedRock is a Redis server.

## Almost All Redis Commmands

Yes exclude module commands. [You can click here for more reference](commands_en.md)

## All Redis Datastruce

Yes.

One of reasons for Redis to be such successfull is its supports for a lot of data structures.

Redis is not like Memcached to support only key/value pair.

Engineers are lazy to write serialized/descerialzed functions, aren't they?

The good thing is RedRock supports all these data structures like Redis.
* String
* Set
* Hash. It is good for memory efficiency, you can check Twitter engineer blog.
* List
* Sorted Set. Like priority queue, the BigO for it is Log(N), close to constant.
* Pub/Sub
* Stream
* HyperLogLog
* Geo

## Support Backup
Yes.

And it is a snapshot. No concern about consistency. [More details](persistence_en.md)

## Cache
Yes.

Use Redis expire(TTL) as before. 

Or just eviction with LRU/LFU. 

As bonus, expire does not need read disk. [Check here](commands_en.md)

## Support multi database
Yes.

Default is the usual 16 database. You can config the number of database you want.

In [the Rocksdb folder](howrun_en.md), you can see the database id as sub-folder name.

## Max Memory Limit
Yes.

RedRock can limit itself to max memory, just like Redis.

[But you need to know, when reaching the limit, what will happen?](howrun_en.md)

## LRU & LFU
Yes.

[More details here](howrun_en.md)

## Replication, i.e., Master/Slave
Yes.

Leader/Follower(i.e., Master/Slave) replica are supported.

## Sentinel
I think it would support when I check the codes.
But I have only one Mac, I can not test it. 
If you test it, please let me know the result. Thank you.

## Cluster
I think so from the codes.
But I have not tested it because I have only one machine.

## Distrubted Lock: RedLock
Yes.

I think it should support from the codes.
When using RedLock, I suggest to check two different ideas on RedLock.

Martin Kleppmann: https://martin.kleppmann.com/2016/02/08/how-to-do-distributed-locking.html

antirez: http://antirez.com/news/101

I support Martin Kleppmann.

## Pipeline
Yes.

Pipeline is the key why Redis get the performance for Million rps in one machine.

## Transaction
Yes.

Including commands like WATCH. [More details](commands_en.md)

## Blocking
Yes. 

[More details](commands_en.md)

## Subcribe/Publish
Yes.

## Stats
Yes. 

[More details](stat_en.md)

## SlowLog
Yes.

But note, because reading disk is commonly take more time than executing only in memory.
The SlowLog's data is heavilly influenced by the reading/writing disk. I am sorry about that.

## Intensive writing to disk
Yes.

Because it is LSM which Rocksdb use. 
From my Mac, I get the metric of 20 MB/s wrting of new data. 
[Check more for Performance](performance_en.md)

## Key/Value length is same as Redis
Yes.

When you caome to SSDB, [a similiar product](peers_en.md), it has the limit of key length of 200 bytes as it encodes the key. 
For Pita，I do not check the detail. But it is based on SSDB. So I doubt the same limit.
[More details](peers_cn.md)

## Temprorary Not Supported Feature -- Module

Right now, I have not supported Redis Module. [Click to Redis website to know more about Redis Module](https://redis.io/topics/modules-api-ref)。

I have not seen too much use cases about Redis Module. I want to save my time for it. Sorry.