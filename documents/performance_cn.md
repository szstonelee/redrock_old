[回中文总目录](menu_cn.md)

# 性能

## 小心假话

我觉得性能应该尽量按坏的情况去考虑，但很多时候，出于商业考虑，很多性能测试报告在说假话。

我今年第一次使用SSD做一个Cache优化时，以为是件很容易的事。因为从网上看到的几个评测，SSD的性能都远远高于HDD。
IOPS可以高达Million级别，Thoughput也轻松逼近1000MBps

但事实是，SSD商家在撒谎（注：现在看，这些话有些偏激，在企业级SSD里，确实在最理想状况下，可以达到一些宣传中的数据）。他们是为了推销自己的产品，拿前几秒，几分钟，甚至特别的测试样例在写测试报告，
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

```
cd metric
mvn package
```

测试程序分两种模式，mode1 和  mode2

## Mode1: 测试时同时做数据验证

### Mode1: 如何运行

#### Server端

##### 先测试原Redis
在Redis官网下载 https://redis.io/
```
sudo ./redis-server --maxmemory 6000000000 --save ""
```
##### 然后测试我们的RedRock
需要用到内存指定

首先预备知识，先请参考: [如何编译](compile_cn.md)，[参数配置](howrun_cn.md)

```
sudo ./redis-server --maxmemory 3000m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```

#### Client端

```
cd metric
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 2 3000 6379
```

这个测试程序的几个参数说明:

* -Xmx9000000000 Java的内存大小, 否则 Java 会 OOM

* 第二个参数, 即 2，表示启动两个线程.

* 第三个参数, 即. 3000, 我们用 3000K (i.e. 3 百万) key/value 数据测试.

* 第四个参数, 即 6379, redis端口. 这个是可选值，缺省值6379.

#### 如何查看有多少比例的数据在磁盘 (只有RedRock才有此功能)

用redis-cli连上RedRock
```
rock report
```

### Mode1结果

#### 纯Redis

Redis内存里的key/value对: 3 百万

| client threads | rps | 95% latency(ms) |
| :-----------: | :-----------: | :-----------: |
| 1 | 16k | 0.07 |
| 2 | 23k | 0.11 |
| 3 | 22k | 0.17 |
| 4 | 34k | 0.16 |
| 5 | 52k | 0.16 |
| 6 | 39k | 0.23 |

```
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 1 3000
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 2 3000
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 3 3000
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 4 3000
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 5 3000
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 6 3000
```

#### RedRock

##### 1 : 4 (23%), for 5 read, 1 to disk, 4 to memory
| client threads | rps | 95% latency(ms) |
| :-----------: | :-----------: | :-----------: |
| 1 | 10k | 0.29 |
| 2 | 17k | 0.33 |
| 3 | 22k | 0.36 |
| 3 | 23k | 0.51 |
| 3 | 22k | 0.66 |
```
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 1 2500
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 2 2500
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 3 2500
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 4 2500
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 5 2500
```


##### 1 : 2 (38%), for 3 read, 1 to disk, 2 to memory
| client threads | rps | 95% latency(ms) |
| :-----------: | :-----------: | :-----------: |
| 1 | 6k | 0.54 |
| 2 | 9k | 0.55 |
| 3 | 8k | 1.03 |

```
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 1 3000
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 2 3000
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 3 3000
```

##### 1 : 1 (56%), 50% to diks, 50% to memory
| client threads | rps | 95% latency(ms) |
| :-----------: | :-----------: | :-----------: |
| 1 | 2k | 1.33 |
| 2 | 3k | 1.23 |
| 3 | 3k | 2.70 |

```
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 1 4000
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 2 4000
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 3 4000
```

##### 4 : 1 (80%), for 10 read, 8 to disk , 2 to memory
```
java -Xmx12000000000 -jar target/metric-1.0.jar mode1 1 7000
```
rps: 0.6k, 95% latency(ms): 4

#### comparison
| server type | rps | 
| :----------- | :-----------: |
| original Redis, all in memory | 52k |
| RedRock, 23% oppertunitiy to disk  | 23k |
| RedRock, 38% oppertunitiy to disk  | 9k |
| RedRock, 56% oppertunitiy to disk  | 3k |
| RedRock, 80% oppertunitiy to disk  | 0.6k |


## Mode2: test read with write in a cache enviroment

待续。。。

## 期待其他环境下的测试

我期望看到其他测试报告，比如：
1. 单纯的Linux(我只用Mac OS上的模拟Ubuntu测试过)，其他Unix，Windows
2. dataset有几百G
3. 其他更高速的SSD，如使用了Raid的SSD
4. 其他参数配置下的RedRock，比如Replacation, Cluster下的RedRock
5. 大键值，比如MB级别的value，或和KB级别的键值混杂