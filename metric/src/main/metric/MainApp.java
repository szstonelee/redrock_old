package metric;

import com.codahale.metrics.ConsoleReporter;
import com.codahale.metrics.MetricRegistry;
import com.google.common.base.Preconditions;

import java.util.LinkedList;
import java.util.List;
import java.util.Random;
import java.util.concurrent.*;

public class MainApp {
    static MetricRegistry metrics = new MetricRegistry();

    private static List<KV> initWarmUpForMode1(int total) {
        long latency;
        latency = System.nanoTime();
        List<KV> list = KVListFactory.warmUp(total);
        latency = System.nanoTime() - latency;
        System.out.println("warmUp latency(ms) = " + String.valueOf(latency/1000000));
        if (list.isEmpty()) {
            throw new IllegalStateException("warm up failed! can not go on");
        } else if (list.size() != total) {
            System.out.println("WarmUp size not equal the input, input = "
                    + total + ", actual = " + list.size());
        }
        return list;
    }

    private static void validateAndMetric(List<KV> list, int threadNumber) {
        BlockingQueue<Runnable> queue = new ArrayBlockingQueue<>(100);
        ThreadPoolExecutor executor =
                new ThreadPoolExecutor(threadNumber, threadNumber,
                        Long.MAX_VALUE, TimeUnit.NANOSECONDS, queue);

        int listLen = list.size();
        Random random = new Random();
        int sleepMs = 1;
        for (int i = 0; i < 1<<20; ++i) {
            int index =  random.nextInt(listLen);
            ValidateTask task = new ValidateTask(index, list.get(index).key, list.get(index).val);
            try {
                executor.execute(task);
                sleepMs = 1;
            } catch (RejectedExecutionException rejectE) {
                // System.out.println("task queue is full at i = " + i + ", sleepMs = " + sleepMs);
                try {
                    Thread.sleep(sleepMs);
                    sleepMs *= 2;
                } catch (InterruptedException interE) {
                    System.out.println("executor submit task to queue interrupted in main thread!");
                }
            }
        }

        executor.shutdown();
        try {
            executor.awaitTermination(30, TimeUnit.SECONDS);
            executor.shutdownNow();
        } catch (InterruptedException e) {
            System.out.println("MainThread interrupted!");
        }
    }

    private static void startReporter(int periord) {
        ConsoleReporter reporter = ConsoleReporter.forRegistry(metrics)
                .convertRatesTo(TimeUnit.SECONDS)
                .convertDurationsTo(TimeUnit.MILLISECONDS)
                .build();
        reporter.start(periord, TimeUnit.SECONDS);
    }

    // local machine
    // ./redis-server --maxmemory 500m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    // java -jar target/metric-1.0.jar mode1 2 2000
    // remote VM
    // sudo ./redis-server --maxmemory 500m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
    // java -jar target/metric-1.0.jar mode1 2 2000 2639
    private static void mode1(int threadNumber, int kvTotal) {
        Preconditions.checkArgument(threadNumber > 0 && threadNumber <= 1000);
        Preconditions.checkArgument(kvTotal > 0 && kvTotal <= 1<<30);


        System.out.println("mode = 1, i.e. validate value, metric only for read");
        System.out.println("Redis port = " + JedisUtils.getRedisPort());
        System.out.println("Thread number = " + threadNumber);
        System.out.println("KV total = " + kvTotal);

        List<KV> list = initWarmUpForMode1(kvTotal);

        startReporter(10);
        validateAndMetric(list, threadNumber);
    }

    private static void printUsage() {
        System.out.println("java -jar target/metric-1.0.jar <mode>, mode == mode1 or mode == mode2");
        System.out.println("if mode1: <thread_number> <total_key_number> <port>");
    }

    // local machine
    // ./redis-server --maxmemory 1000m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    // java -jar target/metric-1.0.jar mode2 2
    // remote VM
    // sudo ./redis-server --maxmemory 1000m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
    // java -jar target/metric-1.0.jar mode2 1
    private static void mode2(int howManyMillion, int threadNumber, int write) {
        Preconditions.checkArgument(howManyMillion > 0 && howManyMillion <= 1000);
        Preconditions.checkArgument(threadNumber > 0 && threadNumber <= 1000);

        MillionKeys millionKeys = MillionKeysFactory.warmUp(howManyMillion);

        List<HalfKiloRpsForMK> jobs = new LinkedList<>();
        List<Thread> threads = new LinkedList<>();

        for (int i = 0; i < threadNumber; ++i) {
            HalfKiloRpsForMK job = new HalfKiloRpsForMK(String.valueOf(i), write, millionKeys);
            jobs.add(job);
            threads.add(new Thread(job));
        }

        for (Thread thread : threads) {
            thread.start();
            try {
                Thread.sleep(31);
            } catch (InterruptedException e) {
                break;
            }
        }

        try {
            for (int i = 0; i < 60; ++i) {
                Thread.sleep(60000);
                int size = jobs.size();
                if (isAllHealth(jobs)) {
                    System.out.println("All = " + size + ", thread work ok, rps = " + jobs.get(0).getRequestInOneSecond()*(double)size/1000 + " krps, we will add new one...");
                    HalfKiloRpsForMK newJob = new HalfKiloRpsForMK(String.valueOf(size), write, millionKeys);
                    jobs.add(newJob);
                    Thread newThread = new Thread(newJob);
                    threads.add(newThread);
                    newThread.start();
                } else {
                    System.out.println("Warning, total thread = " + size + " can not keep all ok, we will decrease one!");
                    if (size > 0) {
                        jobs.get(0).stop();
                        threads.get(0).join();
                        jobs.remove(0);
                        threads.remove(0);
                    }
                }
            }

            for (HalfKiloRpsForMK job : jobs) {
                job.stop();
            }
            for (Thread thread : threads) {
                thread.join();
            }
        } catch (InterruptedException e) {
        }
    }

    private static boolean isAllHealth(List<HalfKiloRpsForMK> jobs) {
        for (HalfKiloRpsForMK job : jobs) {
            if (!job.getHealth()) {
                return false;
            }
        }
        return true;
    }

    public static void main(String[] args) {

        if (args.length < 1) {
            printUsage();
            return;
        }

        String mode = args[0];

        if ("mode1".equals(mode)) {
            int threadNumber = 1;
            if (args.length >= 2) {
                threadNumber = Integer.parseInt(args[1]);
            }
            int kvTotal = 1<<20;
            if (args.length >= 3) {
                int total = Integer.parseInt(args[2]);
                kvTotal = total<<10;
            }
            if (args.length >= 4) {
                int redisPort = Integer.parseInt(args[3]);
                JedisUtils.setRedisPort(redisPort);
            }
            mode1(threadNumber, kvTotal);
        } else if ("mode2".equals(mode)) {
            int howManyMillion = 1;
            if (args.length >= 2) {
                howManyMillion = Integer.parseInt(args[1]);
            }
            int threadNumber = 1;
            if (args.length >= 3) {
                threadNumber = Integer.parseInt(args[2]);
            }
            int write = 1;
            if (args.length >= 4) {
                write = Integer.parseInt(args[3]);
            }
            if (args.length >= 5) {
                int redisPort = Integer.parseInt(args[4]);
                JedisUtils.setRedisPort(redisPort);
                System.out.println("input redis port  = " + redisPort);
            }
            mode2(howManyMillion, threadNumber, write);
        } else {
            printUsage();
        }
    }
}
