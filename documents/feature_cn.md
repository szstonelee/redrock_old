[回中文总目录](menu_cn.md)

# 支持的特性

## 支持的平台

MAC OS和Linux(我在Mac上通过VM模拟的Linux)应该可以。其他我不是太清楚。

## 支持Redis协议

是的。因此，你客户端的代码和配置都不用改。

[可参考测试用例](test_cn.md)

## 支持所有的Redis Commmands

Yes(除了module相关的命令). [你可以参考命令集和注意事项](commands_cn.md)

## 支持所有的Redis数据结构

Redis一个成功的原因，就是它支持大量的数据结构，而不只是简单的key/value pair。
然后自己再serialize/descerilize. 程序员都是懒汉，是不是？
因此，要尽可能支持这些丰富的数据结构，包括：
* String
* Set
* Hast。对于小的hash，可以有效的降低内存消耗。
* List
* Sorted Set。作为类似Priority任务队列，可以O(logN)获得数据
* pub/sub
* stream
* HyperLogLog
* Geo

[可参考测试用例](test_cn.md)

## 支持备份（包括Snapshot）
Yes。[详细参考点这里](persistence_cn.md)

[可参考测试用例](test_cn.md)

## 作为Cache使用
Yes。请用expire特性。而且，其expire不需要读盘，[细节参考这里](commands_cn.md)

## 支持多数据库
Yes。缺省是16个库，也可以配置你想要的数量的库。
在存储目录下，每个库都对应一个Rocksdb的目录，以数字为目录名

## 支持最大内存限制
Yes。这样，RedRock就像Redis一样可以在限定的内存空间里运行。
[但你需要了解如果爆仓了，会出现什么现象](howrun_cn.md)

## 支持LRU & LFU
Yes。[同样细节请看这里](howrun_cn.md)

[可参考测试用例](test_cn.md)

## 支持同步，即Master/Slave
Yes. 我们支持Leader/Follower(就是Master/Slave) replica

[可参考测试用例](test_cn.md)

## 支持Sentinel
我想是的，从代码上看是这样。但我只有一台Mac，搭建不了测试环境。如果你测试通过了，麻烦让我知道。谢谢。

## 支持Cluster
我想是的，从代码上看是这样。但我只有一台Mac，搭建不了测试环境。如果你测试通过了，麻烦让我知道。谢谢。

## 支持Distrubted Lock: RedLock
Yes。
但作者antirez和Martin Kleppmann有不同的看法，你可以参考他们两人的意见。
Martin Kleppmann: https://martin.kleppmann.com/2016/02/08/how-to-do-distributed-locking.html
antirez: http://antirez.com/news/101
我需要做出我自己的判断，我站在Martin Kleppmann这边

## 支持Pipeline
Yes。Pipeline也是单台机器的Redis的Performance能达到Million rps级别的关键原因。

[可参考测试用例](test_cn.md)

## 支持Transaction
Yes。包括Watch这样的命令。[需要留意一个细节](commands_cn.md)

[可参考测试用例](test_cn.md)

## 支持Blocking
Yes。[需要留意一个细节](commands_cn.md)

[可参考测试用例](test_cn.md)

## 支持Subcribe/Publish
Yes。

## 支持统计
Yes。[我们还增加了一个自己的统计。](stat_cn.md)

## 支持SlowLog
Yes。但注意：读盘的时延一般而言会远远高于正常命令的内存执行时间（除非是Keys这样的全浏览命令）。
这也就意味SlowLog没有之前那么准了，引入了读盘的误差，但这个无法避免

## 支持大量写盘
是的。这个是Rocksdb的特性。我自己Mac机器（SSD）测试过，可以到20 MB/s的写入量，是非常可怕的一个数字。
[详细可以参考Performance](performance_cn.md)

## Key/Value长度等同原Redis
Yes.
在一个同类产品SSDB（LevelDb & Rocksdb），因为其对key做了编码，导致key长度现在在200字节。
另外一个Pita，我没有看细节，但其机理应该是SSDB，所以有同样的怀疑。
[详细参考这里](peers_cn.md)

## 暂不支持的功能

暂不支持Redis的Module功能，[点这里去官网了解更多](https://redis.io/topics/modules-api-ref)。

不支持的原因很简单，我没有看到过多这样的需求，暂不准备投入时间处理这个特性。