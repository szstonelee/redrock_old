[回中文总目录](menu_cn.md)

# 配置和运行

Redis可以不用什么配置，命令行下简单执行./redis-server即可。

如果想用RedRock生效，你需要下面几个参数：

## RedRock需要的参数
### maxmemory
首先是必须设置Redis本身的一个参数maxmemory为一个大于0的数值，比如100mb
原因很简单，没有内限制，我们根本就不需要RedRock后面的存储，所以，如果maxmemory==0，意味RedRock特性被禁止。
注意：
1. 设置的maxmemory必须保证能容纳所有的key的大小，比如key平均大小100字节，计划最大的key数量是100M，你需要maxmemroy至少10G的内存
2. maxmemory不能故意太小，比如几兆，因为这么小的容量，RedRock工作起来会很难受，建议至少100M以上
3. 不要maxmemory等于你的所有内存，因为OS也需要内存，Redis的内存限制是不考虑动态产生和消亡的临时工作内存，比如：备份时需要的内存。可以参考官网文档：https://redis.io/topics/memory-optimization，

### enable-rocksdb-feature
当maxmemory != 0时，这个参数设置为yes(缺省是no)，那么意味着RedRock特性起效了

### rockdbdir
这个参数设置RedRock在哪个目录下用Rocksdb作为数据存储。缺省下：/opt/redrock
注意：rockdbdir目录只是临时使用，每次RedRock都会讲这个目录下的东西删除干净，并且停机后，用它作为备份也是不准的，因为内存的东西并不在磁盘上。
如果想备份数据，请参考：[备份持久化](persistence_cn.md)

### maxmemory-only-for-rocksdb
这个是个可选参数，缺省值是yes

#### yes值的意义
当内存使用量逼近maxmemory时，RedRock会尽可能把value存储到存储上，key会永远保存在内存里。
如果最后，几乎所有的value都到了磁盘，那么对于哪些有消耗内存的命令，一般而言，是指有修改的命令，会拒绝服务，某种意义，Redis变成只读的。
这时，如果删除了一些键，腾出内存空间，则修改服务可以继续进行

#### no值的意义
RedRock会尽量把Value存储到存储山上，腾出内存空间，但所有的key还是会保留在内存
直到RedRock精疲力尽，然后就进入到Redis传统的处理方式，有两种结果：
1. 如果Redis配置的是可以删除键腾出空间，Redis将会删除一些key来腾出空间
参考：https://redis.io/topics/lru-cache
2. 如果Redis配置的是不允许遮掩个，Redis将会拒绝服务这个命令，这个命令一般是修改型的命令，意味着Redis变成只读的


### 其他Redis相关参数
参考：https://redis.io/topics/lru-cache
设置为LRU还是LFU，i.e.，上面的算法中，挑选key时（包括RedRock挑选key进行dumping value，以及Redis选择key进行删除），有三种策略
1. LRU：最近使用的key保留
2. LFU：最常使用的key保留
3. Random：每个key都有同等机会被选择

## 如何修改生效参数

有两种方式，一种是改配置文件，另外一种是命令行

### 命令行模式

例如：
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes
```
### 修改配置文件

修改redis.conf文件里，加入enable-rocksdb-feature, rockdbdir, maxmemory-only-for-rocksdb这三个值
可参考：https://redis.io/topics/config

### 检查是否生效

用redis-cli连入，然后执行
```
config get maxmemory
config get enable-rocksdb-feature
config get rockdbdir
config get maxmemory-only-for-rocksdb
```

## 相关问题思考

相关的几个问题读者可以思考一下：
1. 如果Redis不设置maxmemory，实际内存需求又大于机器里的，会发生什么
答案是：Redis会使用操作系统的盘交换区，因为有盘交换区，某种意义上，虚拟内存是无限大的，因为用存储来作为内存的后背
可以参考：https://redis.io/topics/faq#what-happens-if-redis-runs-out-of-memory

2. 那我可不可以用SSD这种快速存储作为操作系统的盘交换区，从而实现最简单的扩展内存方式
答案是Redis作者antierz的一篇文章，[《Redis with an SSD swap, not what you want》](http://antirez.com/news/52)

3. 如果只设置maxmemory，但不设置enable-rocksdb-feature，会如何
这时，RedRock是被禁止的，其功能就是传统的Redis的表现，可以参考：https://redis.io/topics/lru-cache