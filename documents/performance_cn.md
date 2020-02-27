[回中文总目录](menu_cn.md)

# 性能

## 小心假话

我觉得性能应该尽量按坏的情况去考虑，但很多时候，出于商业考虑，很多性能测试报告在说假话。

我今年第一次使用SSD做一个Cache优化时，以为是件很容易的事。因为从网上看到的几个评测，SSD的性能都远远高于HDD。
IOPS可以高达Million级别，Thoughput也轻松逼近1000MBps

但事实是，SSD商家在撒谎。他们是为了推销自己的产品，拿前几秒，几分钟，甚至特别的测试样例在写测试报告，
但基于这样的报告，可能让我们工程师搭建的系统远远达不到指标。
软件呢，也有这个可能。不管你是为了卖软件，还是卖名声，还是为了给老板一个好看的报告。

如果是理想的状况，所有的热键（比如：99.99999%）的访问都是发生在内存里，那么即使99.99999%的数据都在硬盘上，
我也可以宣称RedRock的性能像Redis一样好，因为根本就没有用到磁盘。

但这没有意义。因为RedRock应该考虑的是有大量Miss的情况下，同时大量数据又在磁盘上，性能会怎样？

所以，我下面的测试，是基于一个糟糕的环境假设进行的。

## 测试环境考虑

1. 我们希望大部分key的Value都在磁盘上，至少90%的value都在磁盘上
2. 我们希望访问是随机的，没有热键的差别，即任何一个键都在
3. OS的Page Cache也不能太高，否则，Rocksdb对磁盘的访问，实际都落在了OS对于文件的缓存上。测试时，应该让OS Page Cache低于10%的内存
4. key小，在100字节左右，value大些，在1000字节。我们做了key 20~200的随机， value 200~2000的随机
5. 暂不考虑短连接，即连接了Redis，然后又马上断开。每个请求都这样周而复始
6. 在我的MAC机器上，设置RedRock的内存现在为500M(你自己的机器上，自己调整相关参数)

## 编译执行性能测试程序

请参考metric目录下的测试软件，用java编写

首先用编译好的RedRock启动，[编译见这里](compile_cn.md)，[启动参数说明见这里](howrun_cn.md)

```
./redis-server --maxmemory 500m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```

metric程序的编译和执行
```
cd metric
mvn package
java -jar target/metric-1.0.jar 2 2000
```

其中，第一个2，表示2个线程，第二个2000，表示2000K的数据。这个测试程序执行时间有点久，分钟级。

## 测试结果

1. 95%的键在磁盘上，echo keyreport，可以看到这个报告
2. OS的Page Cache应该和少，我的IDE，浏览器，metric测试程序将大部分内存都用掉了
3. 线程为2时，指标参数最好。线程多了，因为磁盘读写压力太大，反而Thoughput没有那么好
4. rps大致在5k rps，latency 95%在1ms以下

作为对比，如果用同样的metric测试程序，存Redis，rps在60K左右

## 期待其他环境下的测试

我期望看到其他测试报告，比如：
1. 单纯的Linux(我只用Mac OS上的模拟Ubuntu测试过)，其他Unix，Windows
2. dataset有几百G
3. 其他更高速的SSD，如使用了Raid的SSD
4. 其他参数配置下的RedRock，比如Replacation, Cluster下的RedRock
5. 大键值，比如MB级别的value，或和KB级别的键值混杂