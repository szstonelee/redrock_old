package metric;

import java.util.ArrayList;
import java.util.List;
import com.google.common.base.Preconditions;
import redis.clients.jedis.Jedis;
import redis.clients.jedis.Pipeline;
import redis.clients.jedis.Response;

public class KiloRpsForMK implements Runnable {
    private final int BATCH_SIZE = 2;
    private final int ONE_THOUSAND = 500;

    private final int THRESHOLD = 5;
    private final int TOTAL_SECONDS = 50;
    int totalSecondCount;
    int failSecondCount;

    private final String name;
    private volatile boolean stop;
    private volatile boolean health;

    private final int writeInTenth;
    private final MillionKeys millionKeys;

    private long prevWorkNs;
    private int prevExpireCount;
    List<String> prevExpireList;
    List<String> writeList;
    List<String> readList;
    List<Response<String>> responseList;

    KiloRpsForMK(String name, int write, MillionKeys millionKeys) {
        Preconditions.checkArgument(write >= 0 && write <= 5 && write <= BATCH_SIZE);
        this.writeInTenth = write;
        this.millionKeys = millionKeys;

        this.stop = false;
        this.health = true;

        this.totalSecondCount = 0;
        this.failSecondCount = 0;

        this.name = name;
        this.prevWorkNs = 0;

        this.prevExpireCount = 0;
        this.prevExpireList = new ArrayList<>();
        for (int i = 0; i < BATCH_SIZE; ++i) {
            prevExpireList.add("");
        }

        this.writeList = new ArrayList<>();
        for (int i = 0; i < this.writeInTenth; ++i) {
            writeList.add("");
        }

        this.readList = new ArrayList<>();
        this.responseList = new ArrayList<>();
        for (int i = 0; i < BATCH_SIZE - this.writeInTenth; ++i) {
            readList.add("");
            responseList.add(new Response<>(null));
        }
    }

    int getRequestInOneSecond() {
        return this.ONE_THOUSAND;
    }

    private void prepareBatch() {
        int writeIndex = 0;
        for (writeIndex = 0; writeIndex < Math.min(this.writeInTenth, this.prevExpireCount); ++writeIndex) {
            writeList.set(writeIndex, this.prevExpireList.get(writeIndex));
        }
        for (; writeIndex < this.writeInTenth; ++writeIndex) {
            writeList.set(writeIndex, this.millionKeys.randKey());
        }
        for (int i = 0; i < this.readList.size(); ++i) {
            readList.set(i, this.millionKeys.randKey());
        }
    }

    private void batch() {
        prepareBatch();

        try (Jedis jedis = JedisUtils.acquire()) {
            Pipeline p = jedis.pipelined();
            for (int i = 0; i < this.writeInTenth; ++i) {
                p.setex(this.writeList.get(i), this.millionKeys.randTtl(), this.millionKeys.randValue());
            }
            for (int i = 0; i < this.readList.size(); ++i) {
                this.responseList.set(i, p.get(this.readList.get(i)));
            }
            p.sync();
            this.prevExpireCount = 0;
            for (int i = 0; i < this.responseList.size(); ++i) {
                if (this.responseList.get(i).get() == null) {
                    this.prevExpireList.set(this.prevExpireCount, this.readList.get(i));
                    ++this.prevExpireCount;
                }
            }
        }
    }

    private void doOneThousandInOneSecond() {
        long consumeTimeNs = 0;
        int times = ONE_THOUSAND/BATCH_SIZE;
        long sleepMsPerBatch = 0;
        if (this.prevWorkNs < 1000000000) {
            sleepMsPerBatch = (1000000000 - this.prevWorkNs) / 1000000 / times;
        }
        for (int i = 0; i < times; ++i) {
            long now = System.nanoTime();
            batch();
            consumeTimeNs += System.nanoTime() - now;
            if (sleepMsPerBatch > 0) {
                try {
                    Thread.sleep(sleepMsPerBatch);
                } catch (InterruptedException e) {
                }
            }
        }

        this.prevWorkNs = consumeTimeNs;

        ++this.totalSecondCount;
        if (this.prevWorkNs >= 1000000000) {
            ++this.failSecondCount;
        }

        if (this.totalSecondCount >= TOTAL_SECONDS) {
            if (this.failSecondCount >= THRESHOLD) {
                this.health = false;
                // System.out.println("Failed! Thread " + this.name);
            } else {
                this.health = true;
                // System.out.println("Health..., Thread " + this.name);
            }
            this.totalSecondCount = 0;
            this.failSecondCount = 0;
        }
    }

    @Override
    public void run() {
        while (!this.stop) {
            doOneThousandInOneSecond();
        }
    }

    void stop() {
        this.stop = true;
    }

    boolean getHealth() {
        return this.health;
    }
}
