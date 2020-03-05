package metric;

import com.codahale.metrics.ConsoleReporter;
import com.codahale.metrics.MetricRegistry;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.List;
import java.util.Random;
import java.util.concurrent.*;

public class MainApp {
    static MetricRegistry metrics = new MetricRegistry();

    private static int KV_TOTAL = 1<<10;
    private static int QUEUE_LEN =  KV_TOTAL>>2;
    private static int THREAD_NUMBER = 2;

    private static final Logger logger = LoggerFactory.getLogger(MainApp.class);

    private static List<KV> initWarmUp(int total) {
        long latency;
        latency = System.nanoTime();
        List<KV> list = WarmUp.warmUp(total);
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

    private static void validateAndMetric(List<KV> list) {
        int listLen = list.size();
        BlockingQueue<Runnable> queue = new ArrayBlockingQueue<>(QUEUE_LEN);
        ThreadPoolExecutor executor =
                new ThreadPoolExecutor(THREAD_NUMBER, THREAD_NUMBER,
                        Long.MAX_VALUE, TimeUnit.NANOSECONDS, queue);

        Random random = new Random();
        for (int i = 0; i < QUEUE_LEN; ++i) {
            int index =  random.nextInt(listLen);
            Task task = new Task(index, list.get(index).key, list.get(index).val);
            try {
                executor.execute(task);
            } catch (RejectedExecutionException rejectExp) {
                System.out.println("task queue is full at i = " + i);
                try {
                    Thread.sleep(10);
                } catch (InterruptedException interExp) {
                    System.out.println("executor submit task to queue interrupted in main thread!");
                }
            }
        }

        executor.shutdown();
        // long now = System.nanoTime();
        long totalNow = System.nanoTime();;
        int pre = Task.getCounter();
        while (true) {
            try {
                boolean finish = executor.awaitTermination(10, TimeUnit.SECONDS);
                /*
                long latency = System.nanoTime() - now;
                now = System.nanoTime();
                int cur = Task.getCounter();
                System.out.println("wake & check, qps = " +
                        (cur-pre)*1000/(latency/1000000) +
                        ", latency(ms) = " + latency/1000000);

                pre = cur;
                 */
                if (finish) {
                    // System.out.println("All task done!");
                    break;
                }
            } catch (InterruptedException e) {
                System.out.println("MainThread interrupted!");
                break;
            }
        }
        // System.out.println("total  latency(ms) = " + (System.nanoTime() - totalNow)/1000000);
    }

    public static void main(String[] args) {
        // start sudo ./redis-server --maxmemory 500m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
        // java -jar target/metric-1.0.jar 2 2000 (26379）
        ConsoleReporter reporter = ConsoleReporter.forRegistry(metrics)
                .convertRatesTo(TimeUnit.SECONDS)
                .convertDurationsTo(TimeUnit.MILLISECONDS)
                .build();

        if (args.length >= 1) {
            THREAD_NUMBER = Integer.parseInt(args[0]);
        }
        if (args.length >= 2) {
            int total = Integer.parseInt(args[1]);
            KV_TOTAL = total<<10;
            QUEUE_LEN = KV_TOTAL>>2;
        }
        if (args.length >= 3) {
            int redisPort = Integer.parseInt(args[2]);
            JedisUtils.setRedisPort(redisPort);
        }

        System.out.println("Redis port = " + JedisUtils.getRedisPort());
        System.out.println("Thread number = " + THREAD_NUMBER);
        System.out.println("KV total = " + KV_TOTAL);

        List<KV> list = initWarmUp(KV_TOTAL);

        reporter.start(60, TimeUnit.SECONDS);
        for (int i = 0; i < 3; ++i) {
            validateAndMetric(list);
        }
    }
}
