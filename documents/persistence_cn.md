
[回中文总目录](menu_cn.md)

# 备份，持久化 和 同步

如果你希望你的数据安全，你可以使用：备份 或 同步。但对于RedRock，情况非常复杂。我建议你细读下面的技术文档。

## 首先不要用Rocksdb目录进行备份

第一条，也是最重要的，千万不用用Rocksdb目录进行备份。[关于Rocksdb目录](howrun_cn.md). 

原因很简单：内存里的热键和对应的值，是不在Rocksdb目录里的。

你有三钟方法保证你的数据安全。 

如果使用备份, 你可以使用RDB或AOF方式, 就像常规的Redis的备份数据方式, [点击Redis官网了解更多](https://redis.io/topics/persistence). 

第三种方式就是同步. [了解Redis官网关于同步](https://redis.io/topics/replication)

下面有众多的配置，请细读并琢磨。

## 不启动RedRock特性下的备份

如果你[不启动RedRock特性](howrun_cn.md), 你有两种模式备份.

第一种模式是产生一个子进程。第二种模式是直接在Redis主线程里备份。这都是传统的Redis备份模式, [你可以了解更多](https://redis.io/topics/persistence).

子进程模式, 你使用了['BGSAVE'](https://redis.io/commands/bgsave) or ['BGREWRITEAOF'](https://redis.io/commands/bgrewriteaof), 是一种异步结构. 这时其他客户端不受影响，继续得到Redis的服务和响应. 同时Redis采用了COW技术, 备份时的内存消耗不高。

主线程模式，当你使用了['SAVE'](https://redis.io/commands/save), 是同步结构. 所有的客户端都会停止响应直到备份完成。所耗内存不高。 

## 启动RedRock特性下的备份

当你启动了RedRock特性。这两种结构, 同步还是异步, 和Redis是一样的。

但内存消耗大大不同。

### 当配置是： maxmemory-only-for-rocksdb == yes

如果产生了一个子进程备份, 子进程的内存消耗不会超过[maxmemory](howrun_cn.md)。这意味你的最大内存消耗, 即，你的主进程加上你的进程, 都会被限制在这个参数下。但麻烦是备份需要挺长的时间。

如果在主线程里备份，情况类似。只有一个[maxmemory](howrun_cn.md)限制。 不过，此时客户端都不能响应直到备份结束。

### 当配置是： maxmemory-only-for-rocksdb == no

如果是这种情况, [maxmemory](howrun_en.md) 没有作用。 你可以看到子进程或主线程有大量的内存消耗。 

要小心这种情况，因为如果99%的值都在磁盘中，那么可能是原来100倍的内存要求。

## 重启重载备份：不过RedRock特性不起效

当RedRock重新启动时，如果目录下有备份文件，将会重载这个备份文件到内存。

这和传统的Redis一致。 [你可以点这里参考官方文档](https://redis.io/topics/persistence)

所以，小心！如果备份文件里有大量的磁盘内容，当重载时，很可能将耗光你的内存。

## 重启重载备份：RedRock特性起效

### 当配置是： maxmemory-only-for-rocksdb == yes

重启时，你将看到一个子进程，但子进程的内存被现在在[maxmemory](howrun_cn.md)。虽然重载的时间很长，但你的内存是安全的。

### 当配置是： maxmemory-only-for-rocksdb == no

这个配置下，重载是没有内存限制的。重载结束后，RedRock开始缩减内存到[maxmeory](howrun_cn.md)，方法是：要么删除键，要么拒绝一些有内存需求的命令。

## 同步

### 对于master

#### 如果master的RedRock特性没有打开

这种情况下，RedRock就是传统的Redis。 如果同步时不能满足[Redis PSYNC](https://redis.io/commands/psync), master会产生一个子进程先产生一个rdb文件，并且用到COW技术。所以内存消耗不大。可以了解官网[Redis Sync](https://redis.io/commands/sync) 和 [Redis Replication](https://redis.io/topics/replication)

但因为master没有RedRock特性, 也就意味数据集不大，只能够内存装下那么大。

#### 如果master的RedRock已经生效

如果PSYNC模式可以, 会使用psync。但我们用了RedRock特性，所以数据集应该是很大的，所以一般而言PSYNC是无法起效的。

因此只能使用SYNC模式。RedRock会生成一个子进程产生rdb文件。子进程的内存消耗将限制在maxmemory. 好消息是内存没有毛病。但会有下面几个坏消息。

首先，生成rdb文件的时间会相当长。

其次, 在这段时间里, master必须开辟一个缓存，用于积累客户端新产生的命令。这个缓存有可能变得很大，因为前面说这个文件生产时间很长，如果这段时客户端不停发来命令的话。而且这个缓存的大小是不受maxmemory的约束的。 

### 对于slave

#### 如果slave的RedRock特性没有打开

就算设置了maxmemory，slave也没有内存限制。所以，小心，如果数据集很大，你的内存会不够用。

#### 如果slave的RedRock特性打开

如果是这个配置，那么slave的内存将限制在maxmemory。内存没有问题了，但同步时间会变得漫长（天下没有双全的事）。

### 如何做到PSYNC完美模式

如果mastre/slave两边的数据差别很小，那么同步将采用PSYNC模式，这种模式下，是增量数据传递，我们不用担心内存的消耗。

如果是这样，那么master/slave都可以启用RedRock特性。
‘
这是多么美的一件事。 

但前提是：

1. master/slave之间的网络连接很流畅，无中断，或者中断马上恢复。

2. 一开始数据集不大时，master/slave就已经连接好了。

## 总结

由于RedRock要处理复杂的内存和磁盘平衡问题，所以，产生了众多的配置和对应的运行状况。

建议你仔细阅读，用心规划，并做完整测试。

## 备份的一致性问题

备份开始，数据就是一个快照。所以，之后的修改不会进入到备份文件。因此，不用担心备份数据一致性问题。