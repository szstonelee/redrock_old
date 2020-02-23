[回英文总目录](../README.md) 

# 简介

RedRock，就是 Redis + Rocksdb.

Redis是个内存性的NOSQL，但内存比较贵，我们希望：
* 有Redis的基于内存的快速
* 有磁盘存储作为后背从而有无限空间。

当前SSD的价格和性能都很吸引人，这也是本项目产生的原因。 

它具有如下的几个特点：
* 不启动新特性下，代码执行和Redis源代码一模一样
* 支持Redis所有的命令
* 支持Redis的所有的数据结构，包括：String,List,Set,Hash,SortedSet,Stream,HyperLogLog
* 支持备份和持久化，包括：全量RDB, 增量AOF；可选是否子进程备份
* 支持可配置最大内存使用量限量
* 支持主从模式，leader & follower applica
* 我猜测应该支持哨兵集群，Sentinel
* 我猜测应该支持分区集群，Cluster
* 支持管道Pipeline处理
* 支持事务Transaction
* 支持阻塞操作Blocking
* 支持原有的所有统计，同时有自己的统计
* 支持慢操作的日志
* 开启新特性后，特别能容纳大量写
* 如何访问主要是适合内存热键，性能和Redis一样，即一台机器Million级别的rps
* 最坏的平均随机访问情况下，性能下降估计仅一个数量级，i.e 百krps
* 增加了Rocksdb库，但其内存使用量很低，只有几十兆
* 维持Redis核心命令和核心逻辑单线程逻辑，避免了同步锁的代价和风险

# 编译

[基于C/C++，详细点这里](compile_cn.md)

# 配置和运行

[Redis的配置基础上，再加上我们新增的几个配置参数，如何配置运行](howrun_cn.md)

# 支持的Redis命令

[支持Redis所有的命令，需要注意的细节请点入](commands_cn.md)

# 支持Redis的特性

[可能支持Redis的所有的特性，比如主从，集群，事务](feature_cn.md)

# 测试

[针对各种场景的测试](test_cn.md)

# 性能

[在最坏的情况下的性能表现](performance_cn.md)

# 备份（持久化）

[内存数据能备份](persistence_cn.md)

# 同类产品

[网上其他类似产品](peers_cn.md)