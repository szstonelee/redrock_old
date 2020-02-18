package metrics;

import com.codahale.metrics.Timer;
import redis.clients.jedis.Jedis;

import java.util.concurrent.atomic.LongAdder;

public class Task implements Runnable {
    private final static LongAdder counter = new LongAdder();
    private final static Timer timer = MainApp.metrics.timer("latency");

    private final int index;
    private final String key;
    private final String val;

    Task(int index, String key, String val) {
        this.index = index;
        this.key = key;
        this.val = val;
    }

    @Override
    public void run() {
        try (Jedis jedis = JedisUtils.acquire()) {
            Timer.Context context = timer.time();
            try {
                String redisVal = jedis.get(key);
                if (redisVal.equals(this.val)) {
                    counter.increment();
                } else {
                    System.out.println("value not equal! index = " + index + ", key = " + key + ", redis val = " + redisVal + ", original val = " + val);
                }
            } catch (Exception e) {
                System.out.println("Exception in thread run! key = " + this.key + ", exception = " + e.toString());
            } finally {
                context.stop();
            }
        }
    }

    static int getCounter() {
        return counter.intValue();
    }
}

