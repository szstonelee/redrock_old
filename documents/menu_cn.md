[英文总目录可点这里](../README.md) 

# 简介

RedRock，简而言之，就是 Redis + Rocksdb。

Redis是个内存性的NOSQL，但内存比较贵，我们希望：
* 有Redis的基于内存的快速
* 有磁盘存储作为后背从而有无限空间。

当前SSD的价格和性能都很吸引人，这也是本项目产生的原因。 

它具有如下的一些特点：
* 不启动新特性下，代码执行和Redis源代码一模一样
* 因为使用Redis Protocol，所以你以前的代码一行都不用改，包括任何配置文件
* 所有的Key都在内存里，但只有热键的值在内存里，所以大部分的值都在磁盘里，因此dataset可以远远大于内存
* 因此你的数据集可以一百倍于你的内存
* 支持Redis几乎所有的命令(当前暂时还不支持module相关的命令)
* 支持键值过期或直接删键，所以，你可以将之作为一个Cache
* 支持Redis的所有的数据结构，包括：String,List,Set,Hash,SortedSet,Stream,HyperLogLog,Geo
* 支持备份和持久化，包括：全量RDB, 增量AOF；可选是否子进程备份
* 支持可配置最大内存使用量限量
* 支持用LRU或LFU算法选择记录入盘
* 如果你采用删除策略，你可选LRU/LFU，同时可指定有多少Key和Value一起在内存里保留
* 支持主从模式，Leader/Follower(也就是Master/Slave) replica
* 我认为应该支持哨兵集群，Redis Sentinel
* 我认为应该支持分区集群，Redis Cluster. 如果这样，就不需要Twemproxy或者Codis
* 我认为应该支持Redis的分布锁，但我支持Martin Kleppmann关于Redis分布锁的看法 
* 支持管道Pipeline处理
* 支持事务Transaction，Watch命令也支持
* 支持阻塞操作Blocking
* 支持LUA这样的Script语言
* 支持订阅/发布Publication/Subscribe
* 支持Redis的流(Stream)处理
* 支持原有的Redis所有统计(含慢操作的日志)，同时有自己的统计
* 开启新特性后，特别能容纳大量写，对于常规的SSD，可以达到每秒20MB的新写入
* 数据都是压缩存在磁盘上的，只需要10%到50%这样大的数据集磁盘容量即可
* 如何访问主要是适合内存热键，性能和Redis一样，即一台机器Million rps
* 最坏的平均随机访问情况下，性能下降估计仅一个数量级，我估测一台机器百K rps
* 增加了Rocksdb库，但其内存使用量很低，只有几十兆
* 维持Redis核心命令和核心逻辑单线程逻辑，避免了同步锁的代价和风险
* key和value大小都和Redis一样，因此没有特别的key大小限制
* 支持多个库，并且可配置

# 编译

[基于C/C++，详细点这里](compile_cn.md)

# 配置和运行

[支持所有的Redis的已有的配置上，再加上我们新增的四个（其中有些是可选的）配置参数，如何配置运行](howrun_cn.md)

# 支持的Redis命令

[支持Redis所有的命令(除了module相关的命令)，需要注意的细节请点入](commands_cn.md)

# 支持Redis的特性

[可能支持Redis的所有的特性，比如主从，集群，事务](feature_cn.md)

# 测试

[针对各种场景的测试](test_cn.md)

# 性能

[在最坏的情况下的性能表现](performance_cn.md)

# 备份（持久化）还有同步

[这些都是为了你的数据安全，点这里了解更多](persistence_cn.md)

# 统计和工具

[一些统计用的工具](stat_cn.md)

# 同类产品

[网上其他类似产品](peers_cn.md)