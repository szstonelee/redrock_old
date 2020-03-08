[Back Top Menu](../README.md)

# Performance

## Be care of lies

I think when you come to performance, you need treat it in the bad simutation, not the best situation.

Best situation lies. 

This year, when I first used SSD to improve a cache system, I thought it was a easy job because I checked a lot of metric reports online which told me that it is easy to achive Million IOPS and nearly 1G Bps throughput for SSD. 

But they lie.

It is reaonable for a hardware merchant to lie because it is a business.

But for our software engineer, we also have pressures to lie or only show good metrics.

For selling, for community fame, or for promotion. Who knows?

If in a best situation, with 99.99999% key/value visits hitting the memory, even with 99.99999% keys' value in disk,
I could say RedRock is just as fast as Redis because it is nearly a Redis user case. 

But it does not make sense. We need to consider the bad or worst situation.

If 99.99999% miss in memory, what is my real system performance?

## Test Enviroment

1. We hope most key's value in disk, at least 90%. (Sometimes I tested in 99%)
2. We hope visits are random for all keys. No specific hot keys with its value living in memory for a long time.
3. Page cache for OS can not be high. Otherwise, reading from disk is actually reading from memory.
4. Key is small than value. Key size random from 20 to 200 bytes. Value size random from 200 to 2000 bytes.
5. Does not consider short connection. 

My Mac 16G DDR memory, 4 Core 2.2GHz Intel i7, 250G PCI SSD.

## Compile Metric Program

Please reference the program in metric sub folder, written in Java.

```
cd metric
mvn package
```

The metric program support two kinds of test mode, mode1 and mode2.

## Mode1: test only reads with value validatation

### Mode1: run RedRock

First start RedRock using the following config parameters，

You can reference: [How compile](compile_en.md)，[Config parameters manual](howrun_en.md)

```
./redis-server --maxmemory 500m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```

### Mode1: how run metric
```
cd metric
java -jar target/metric-1.0.jar mode1 2 2000 6379
```

About the parameters of the metric program:

* First parameter, i.e. 2，meaning 2 concurrent threads.

* Second parameter, i.e. 2000, meaning 2000K key/value pair entries.

* third parameter, i.e. 6379, is the redis port. This parameter is optional with default value 6379.

It will take a while for the test. You can have a cup of coffee.

### Mode1: metric results

+ 95% key's value in disk. You can use 'redis-cli', run such command to get the result 
```
rock report
```
+ Page cache for OS needs to to be low, usually around 1G. My browser, IDE, VM and the metric program used a lot of memory.
+ It is the best result for 2 thread for my Mac. More threads mean more pressures and worse results.
+ rps is around 5K. 95% latency below 1ms.

As a comparsion, when I run the metric again a real redis. rps is about 60K.

## Mode2: test read with write

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
The first parameter, is how many million key/value in the Cache.
The second parameter, is how many thread when start. 
The third parameter, is how many write in tenth quest, 1 meaning 10%, 2 meaning 20%.
The fourth parameter, is the Redis server port. It is optional.

### Mode3: metric results

the following table:


## Excpect other test cases

1. real Linux (I only tested performance in my MAC OS), Unix, Windows
2. dataset as large as close to 1TB
3. more powerful SSD, like SSD using Raid
4. other situations like Redis Cluster
5. big values like MB value or mix with KB value