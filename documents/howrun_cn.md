[回中文总目录](menu_cn.md)

# 配置和运行

Redis可以不用什么配置，命令行下简单执行./redis-server即可。

提醒：很多地方运行都需要sudo权限，建议你将自己的账号提升为sudo
```
su <你的账号>
```

如果想用RedRock功能生效，你需要下面几个参数：

## RedRock需要的参数
### 1. maxmemory
首先是必须设置Redis本身的一个参数maxmemory为一个大于0的数值，比如100mb

原因很简单，没有内限制，我们根本就不需要RedRock后面的存储 

所以，如果maxmemory==0，意味RedRock特性被禁止。 

注意：
1. 设置的maxmemory必须保证能容纳所有的key的大小，比如key平均大小100字节，计划最大的key数量是100M，你需要maxmemroy至少10G的内存
2. maxmemory不能故意太小，比如几兆，因为这么小的容量，RedRock工作起来会很难受，建议至少100M以上
3. 不要maxmemory等于你的所有内存，因为OS也需要内存，Redis的内存限制是不考虑动态产生和消亡的临时工作内存，比如：备份时需要的内存。可以参考官网文档：https://redis.io/topics/memory-optimization
4. 当enable-rocksdb-featurew为“yes"时，maxmemory一旦修改为0，是不允许再改回0

### 2. enable-rocksdb-feature
当maxmemory != 0时，这个参数设置为yes(缺省是no)，那么意味着RedRock特性起效了

### 3. rockdbdir
#### 缺省值
这个参数设置RedRock在哪个目录下用Rocksdb作为数据存储。缺省下：./redrock_rocksdb/ 

如果你缺省在src下编译运行，那么redrock_rocksdb是src的子目录。

注意：
1. 目录名最后一个字符必须是'/' 
2. rockdbdir目录只是临时使用，每次RedRock都会讲这个目录下的东西删除干净
3. 停机后，用此目录作为备份也是不准的，因为内存的东西并不在磁盘上
4. 如果想真正备份数据，请参考：[备份持久化](persistence_cn.md)
#### Linux下的目录权限

注意：如果你自己设置自定义derockdbdir目录，比如：/opt/redrock_rocksdb/，请设置好相应的目录权限。 

如果权限不对，你将看到下面这个错误信息
```
rockapi write status = IO error: while open a file for lock: /opt/redrock_rocksdb/0/LOCK: Permission denied
```

解决上面的错误，你有三个方法：

1. 使用其他你有权限的目录，并配置到rockdbdir这个参数
2. 用sudo来启动你的程序
```
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes
```
3. 用chmod修改缺省目录的权限 
```
sudo chmod -R 777 /opt/redrock_rocksdb
```


### 4. maxmemory-only-for-rocksdb
这个是个可选参数，缺省值是yes

#### yes值的意义
1. 当内存使用量逼近maxmemory时，RedRock会尽可能把value存储到存储上，key会永远保存在内存里。 
2. 如果最后，几乎所有的value都到了磁盘，那么对于哪些有消耗内存的命令，一般而言，是指有修改的命令，会拒绝服务，某种意义，Redis变成只读的。
3. 如果删除了一些键，腾出内存空间，则所有命令又可以继续得到进行

#### no值的意义
RedRock会尽量把Value存储到存储山上，腾出内存空间，但所有的key还是会保留在内存 
直到RedRock精疲力尽，然后就进入到Redis传统的处理方式，有两种结果：
1. 如果Redis配置的是可以删除键腾出空间，Redis将会删除一些key来腾出空间 
参考：https://redis.io/topics/lru-cache
2. 如果Redis配置的是不允许删除键，Redis将会拒绝服务这个命令，这个命令一般是修改型的命令，意味着Redis变成只读的

#### 5. max-hope-hot-keys
只有maxmemory-only-for-rocksdb == no，'max-hope-hot-keys'才生效。

此参数可选，默认值是1000.

其生效时的意义是：

当内存接近极限mammemory时，RedRock会删除一些键，以便腾出内存空间，但RedRock会尽一切可能（但不保证100%）保留'max-hope-hot-keys'的键和值都在内存里不被删除。这些被保留的热键的挑选，是根据LRU/LFU算法决定，当你配置Redis时（参考下面的内容）或者 [点这里了解Redis的删键算法](https://redis.io/topics/lru-cache)

上面这段话挺绕的，我举个例子给你听：

比如你配置RedReck的maxmemory是100兆，你准备增加3百万个键值到RedRock里，一般键很小，就几十字节，值稍微大些，比如是几千字节。

如果maxmemory-only-for-rocksdb == yes，RedRock将不删除键，尽一切可能将这些键和值都塞进你配置的100兆内存空间里。但显然这是不可能完成的任务，因为光键的总和就已经超过了100兆。最后，RedRock在塞不下的情况下（内存里几乎所有的值都在库中），剩下的键，它只能拒绝服务了（RedRock will say sorry）。

那我们将maxmemory-only-for-rocksdb改为'no'会如何？

这时，为了能完成后面更多键的加入，RedRock开始删除一些键值来腾出空间。你猜想应该是通过LRU/LFU来挑选键进行删除，其实不是的，因为这些信息都在值上，而此时，几乎所有的值都在磁盘里，RedRock无法计算，因此，实际删除是随机的。

这就是我们为什么要设置'max-hope-hot-keys'这个参数。当设置了这个参数（不为0的情况下），RedRock将会尽力保持max-hope-hot-keys这么多的键和值都在内存里。而且这些键是通过LRU/LFU算法来选择的。比如：max-hope-hot-keys == 1000, 然后你尝试加入3百万个键值，这个时间挺长，你的其他客户端同时也在访问其中一些键（比如只访问其中固定的1000个），因此这些键会更热些（满足LFU/LRU条件）。当3百万个键值加完后，时间只有2百万个key保留了，另外1百万个键值被删除了。但神奇的是，那些被其他客户端访问的1000个值，几乎都得到了保留（大概是99.5%的保留率）而没有被删除。

这就是'max-hope-hot-keys'的意义和魅力！

### 其他Redis相关参数
参考：https://redis.io/topics/lru-cache 
设置为LRU还是LFU，i.e.，上面的算法中，挑选key时（包括RedRock挑选key进行dumping value，以及Redis选择key进行删除），有三种策略
1. LRU：最近使用的key保留
2. LFU：最常使用的key保留
3. Random：每个key都有同等机会被选择

## 如何修改生效参数

有两种方式，一种是改配置文件，另外一种是命令行

### 命令行模式

#### 范例1: 100M Max Memory and No Eviction
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes
```
#### 范例2：100M Max Memory and Evicition Randomly
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --maxmemory-policy allkeys-lfu
```
#### 范例3：100M Max Memory and Eviction with LFU of at least 10K Keys
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --maxmemory-policy allkeys-lfu --max-hope-hot-keys 10000
```

### 修改配置文件

修改redis.conf文件里，加入enable-rocksdb-feature, rockdbdir, maxmemory-only-for-rocksdb, max-hope-hot-keys这四个值 
```
./redis-server redis.conf
```
可参考：https://redis.io/topics/config

### 不允许在线即时修改

你不能用 "config set" 命令去在线修改上面的参数，除了’maxmemory‘和'max-hope-hot-keys'. 

### 检查是否生效

用redis-cli连入，然后执行
```
config get maxmemory
config get enable-rocksdb-feature
config get rockdbdir
config get maxmemory-only-for-rocksdb
config get max-hope-hot-keys
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