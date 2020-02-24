[回中文总目录](menu_cn.md)

# 支持的Redis命令

## Redis的命令集合

Redis的命令非常丰富，请参考：https://redis.io/commands

我还没有发现哪些Redis的命令RedRock不能支持，如果你发现有问题，麻烦通知我，谢谢。

## 值在磁盘上也不影响效率的命令
对于一些命令，即使其key对应的值在磁盘上，也不会影响其效率，见下表：
比如：set
如果其值在磁盘上，但因为对应的value是overwrite，所以，它不会产生任何读盘的操作，直接用新值取代磁盘上的旧值

下面是这些命令的清单
* set
* setnx
* setex
* psetex
* del
* unlink
* exists
* mset
* msetnx
* randomkey
* expire
* expireat
* pexpire
* pexpireat
* keys
* scan
* multi
* discard
* ttl
* touch
* pttl
* persist
* watch
* unwatch
* restore
* restore-asking
* asking

## 和key无关的命令
有一些命令，其和值无关，因此理论上，这些不会影响
* module
* select
* swapdb
* dbsize
* auth
* ping
* echo
* shutdown
* lastsave
* replconf
* flushdb
* flushall
* info
* monitor
* slaveof
* replicaof
* role
* config
* cluster
* readonly
* readwrite
* memory
* client
* hello
* slowlog
* script
* time
* wait
* command
* post
* host
* latency
* lolwut
* acl

## 和备份有关的命令

备份是要对所有的值进行采用，而且还有一致型的考虑，因为备份是个长时间的过程

* save
* bgsave
* bgrewriteaof

请参考：[内存数据能备份](persistence_cn.md)

注意：以下命令和两台机器同步有关，和备份类似

* sync
* psync

## 不会缓存key到磁盘上命令
有一些命令对应的值是永远不会缓存到磁盘上的
* subscribe
* unsubscribe
* psubscribe
* punsubscribe
* publish
* pubsub
* xadd
* xrange
* xrevrange
* xlen
* xread
* xreadgroup
* xgroup
* xsetid
* xack
* xpending
* xclaim
* xinfo
* xdel
* xtrim

## 其他命令需要读盘，但需要特别注意的有

剩下的命令，都可能需要读磁盘已恢复内存里被dump出去的值，但有一些需要特别注意

### 事务：exec命令
这个是Transaction命令，其读盘是根据本次事务里是所有的命令的全集，是否有key会读盘决定的，
所以，估算exec的影响，你应该对事务中的每条命令的和磁盘读写的相关性来决定

比如：

multi
set k1, v1
set k2, v2
exec

因为set不受磁盘影响，因此此transaction也不会受影响

### Blocking相关的命令

和Blocking相关，都是要先读出磁盘的值，再决定是否unblock

比如：

client A, block on 两个key， 比如k1，k2
这时，k1是空的，但k2的值在磁盘上，因此需要读盘
但k2读值成功之前，有一个新客户向磁盘里写更多的东西，同时也产生了k1
这时，并不导致Client A的解锁，而需要k2读取成功后才会计算解锁

