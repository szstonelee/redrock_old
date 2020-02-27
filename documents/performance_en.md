[Back Top Menu](../README.md)

# Performance

## Be care of lies

I think when you come to performance, you need treat it in the bad simutation, not the best situation.

Best situation lies. 

This year, when I first used SSD to improve a cache system, I thought it was a easy job because I checked a lot of metric reports online which told me that it is easy to achive Million IOPS and nearly 1K MBps throughput for SSD. 

But they lies.

It is reaonable for the hardware merchants to lie because they sell.

But for our software engineer, we also have pressure to lie or only show good metrics.

For sell our software, for self-own fame online, or for promotion. Who knows?

If in a best situation of 99.99999% key/value visits hitting the memory, even with 99.99999% keys' value in disk,
I could say RedRock is just as fast as Redis (because it is nearly a Redis user case!!!) 

But it does not make sense. We need to consider the bad or worst situation.

If 99.99999% miss in memory, what is my real system performance?

## Test Enviroment

1. We hope most key's value in disk, at least 90%. (I tested in 95%)
2. We hope visits are random for all keys. No hot key for relative longer time.
3. Page cache for OS can not be high. Otherwise, reading from disk is actually reading from memory.
4. Key is small than value. Key size random from 20 to 200 bytes. Value size random from 200 to 2000 bytes.
5. Does not consider short connection. 
6. From my specific Mac, I set maxmemory of 500M for my test.
7. My Mac 16G DDR memory, 4 Core 2.2GHz Intel i7, 250G PCI SSD.

## Compile Metric Program

Please reference the program in metric sub folder, written in Java.

First start RedRock using the following config parameters，

[How compile](compile_en.md)，

[Config parameters manual](howrun_en.md)

```
./redis-server --maxmemory 500m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```

how metric compile and run
```
cd metric
mvn package
java -jar target/metric-1.0.jar 2 2000
```

For the parameter of metric.
First parameter 2，meaning 2 concurrent threads.
Second parameter 2000, meaning 2000K key/value pair entries.

It will take a while for the test. You can have a cup of coffee.

## Metric Results

1. 95% key's value in disk. 'echo keyreport' for the result. 
2. Page cache for OS may be low, because a lot of memroy is used by my browser, IDE and the metric program.
3. It is the best result for 2 thread for my Mac. More threads mean more pressures.
4. rps is around 5K. 95% latency below 1ms.

As a comparsion, when I run the metric again a real redis. rps is about 60K.

## Excpect other test cases

1. real Linux (I only tested performance in my MAC OS), Unix, Windows
2. dataset as large as close to 1TB
3. more powerful SSD, like SSD using Raid
4. other situations like Redis Cluster
5. big values like MB value or mix with KB value