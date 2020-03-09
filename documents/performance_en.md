[Back Top Menu](../README.md)

# Performance

## Be careful of lies

I think when you come to performance, you need treat it in the bad simutation, not the best situation.

Best situation lies. 

This year, when I first used SSD to improve a cache system, I thought it was a easy job because I checked a lot of metric reports online which told me that it is easy to achive Million IOPS and nearly 1G Bps throughput for SSD. 

But they lie.

It is reaonable for a hardware merchant to lie because it is a business.

But for our software engineer, we also have pressures to lie or only show good metrics.

For selling, for community fame, or for promotion. Who knows?

If in a best situation, with 99.99999% key/value visits hitting the memory, even in the condition like 99.99999% keys' value in disk,
I could say RedRock is just as fast as Redis because it is nearly a Redis user case. 

But it does not make sense. We need to consider the bad or worst situation.

If 99.99999% miss in memory, what is my real system performance?

## Test Enviroment

1. We hope most key's value in disk, for severl G dataset, at least 90%. (Sometimes I tested in 99%).
2. The key factor for test is how many percentage of oppertunity a key will be read from disk.
3. We hope visits are random for all keys. No hot keys.
4. Page cache for OS can not be high. Otherwise, reading from disk is actually reading from memory.
5. Key is small than value. Key size random from 20 to 200 bytes. Value size random from 200 to 2000 bytes.
6. Does not consider short connection. 

My Mac 16G DDR memory, 4 Core 2.2GHz Intel i7, 250G PCI SSD.

## Compile Metric Program

Please reference the program in metric sub folder, written in Java.

```
cd metric
mvn package
```

The metric program support two kinds of test mode, mode1 and mode2.

## Mode1: test only reads with value validatation

### Mode1: How run

#### the server

##### Original Redis
You can download original Redis from https://redis.io/
```
./redis-server --maxmemory 6000000000 --save ""
```
##### RedRock
run RedRock in specific memory

First start RedRock using the following config parameters，

You can reference: [How compile](compile_en.md)，[Config parameters manual](howrun_en.md)

```
./redis-server --maxmemory 3000m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
NOTE: if linux, You need add 'sudo'.

#### the client

```
cd metric
java -Xmx9000000000 -jar target/metric-1.0.jar mode1 2 3000 6379
```

About the parameters of the metric program:

* -Xmx9000000000 means 9G for Java runtime, otherwise, Java wil OOM

* Second parameter, i.e. 2，meaning 2 concurrent threads.

* Third parameter, i.e. 3000, meaning 3000K (i.e. 3 million) key/value dataset.

* Fourth parameter, i.e. 6379, is the redis port. This parameter is optional with default value 6379.

It will take a while for the test. You can have a cup of coffee.

#### Percentage of value in disk (only for RedRock)

use redis-cli to connect RedRock, then
```
rock report
```

### Mode1: Results

#### Original Redis

Condition: How many keys&values in Redis memory: 3 Million

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

This time, we do not care validation. We believe the data is correct. 

Now we test something like Cache.

We warm up the RedRock with some key/value with expire TTL. Then the metric program try to start several threads to visit the RedRock like Cache. Each thread will try to at the speed 1 K rps with pipeline of ten. Key size is between 20 - 200 bytes. Value size is between 200 - 2000 bytes. If every thread can keep the rate of 1K rps, the program will add one more thread every minute. If it can not maintain the load for each thread to keep 1 Krps, it will decrease one thread. So you can conclude how many rps your Cache will support. You can specify the write percentage. If write == 1, it means 10% write of total load.

### Mode2: run RedRock

```
./redis-server --maxmemory <x>000m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
x: 1 - anything you want, if x == 1, it means 1G memory.

If 1 G memory, it can support 1 million with around 40% key/value in disk. 2 million with around 80% key/value. 3 million around 90%

Check the followong 'how run metric' for how config.

### Mode2: how run metric
```
cd metric
java -jar target/metric-1.0.jar mode2 2 5 1 6379
```
The second parameter, is how many million key/value in the Cache.
The third parameter, is how many thread when start. 
The fourth parameter, is how many write in tenth quest, 1 meaning 10%, 2 meaning 20%.
The fifth parameter, is the Redis server port. It is optional.

### Mode3: metric results

the following table:


## Excpect other test cases

1. real Linux (I only tested performance in my MAC OS), Unix, Windows
2. dataset as large as close to 1TB
3. more powerful SSD, like SSD using Raid
4. other situations like Redis Cluster
5. big values like MB value or mix with KB value