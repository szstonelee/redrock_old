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

If in a best situation, 99.99999% key visit hit the memory, and even as 99.99999% key's value in disk,
I could say RedRock is just as fast as Redis (because it is a Redis user case!!!) 

But it does not make sense. We need to consider the bad situation.

If 99.99999% miss, what is my system performance?

## Test Enviroment

1. We hope most key's value in disk, at least 90%.
2. We hope visit is randomly for all keys. No hot key.
3. Page cache for OS can not be high. Otherwise, you read from disk, but actually you read from OS page cache memory.
4. Key is small than value. Key size random from 20 to 200 bytes. Value size random from 200 to 2000 bytes.
5. Does not consider short connection. 
6. From my specific Mac, I set maxmemory of 500M for my test.

My Mac 16G DDR3 memory, 4 Core 2.2GHz Intel i7, 250G PCI SSD.

## Compile Metric Program

Please reference the program in metric sub folder, written in Java.

First start RedRock using the following config parameters，[How compile](compile_en.md)，[Config parameters manual](howrun_en.md)

```
./redis-server --maxmemory 500m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```

how metric compile and run
```
cd metric
mvn package
java -jar target/metric-1.0.jar 2 2
```

For the parameter of metric.
First 2，meaning 2 Million keys.
Second 2, meaning two concurrent threads.

It will take a while for the test. You can have a cup of coffee.

## Metric Results

1. 95% key's value in disk. 'echo keyreport' for the result. 
2. Page cache for OS may be low, because a lot of memroy is used by my browser, IDE and the metric program.
3. When thread number comes to 2, it is the best result for throughput. More threads mean more pressure for the disk and not good for the performance result.
4. rps is around 5K. 95% latency below 1ms.

As a comparsion, when I run the metric again a real redis. rps is about 60K.

## Excpect other test cases

1. Linux
2. dataset as large as close to 1TB
3. more powerful SSD, SSD using Raid
4. other situation like Redis Cluster
5. big value like MB value or mix with KB value