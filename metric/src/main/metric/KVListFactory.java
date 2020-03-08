package metric;

import com.google.common.base.Preconditions;
import org.apache.commons.lang3.RandomStringUtils;
import redis.clients.jedis.Jedis;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Random;

class KVListFactory {
    private static final int KS_LOWER = 20;
    private static final int KS_UPPER = 201;
    private static final int VS_LOWER = 200;
    private static final int VS_UPPER = 2001;

    private static final int TRY_UPPER_BOUND = 100000;

    private static final Random rand = new Random();

    private static int randNum(int lower, int upper) {
        return rand.nextInt(upper-lower) + lower;
    }

    private static List<KV> createList(int num) {
        Preconditions.checkArgument(num > 0);

        List<KV> list = new ArrayList<>(num);

        for (int i = 0; i < num; ++i) {
            String key = RandomStringUtils.randomAlphanumeric(randNum(KS_LOWER, KS_UPPER));
            String val = RandomStringUtils.randomAlphanumeric(randNum(VS_LOWER, VS_UPPER));
            list.add(new KV(key, val));
        }

        return list;
    }

    static List<KV> warmUp(int totalLen) {
        List<KV> list = createList(totalLen);
        int maxTry = 0;

        try (Jedis jedis = JedisUtils.acquire()) {
            jedis.flushDB();
            Thread.sleep(5000);
            for (KV item : list) {
                int tryCount = 0;
                while (tryCount < TRY_UPPER_BOUND) {
                    try {
                        jedis.set(item.key, item.val);
                        break;
                    } catch (redis.clients.jedis.exceptions.JedisDataException e) {
                        ++tryCount;
                    }
                }
                if (tryCount > maxTry) {
                    maxTry = tryCount;
                    System.out.println("maxTry = " + String.valueOf(maxTry));
                    if (maxTry >= TRY_UPPER_BOUND) {
                        System.out.println("too many retry!");
                        return Collections.emptyList();
                    }
                }
            }
        } catch (InterruptedException e) {
        }

        return list;
    }

}

