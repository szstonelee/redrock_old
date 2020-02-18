package metrics;

import redis.clients.jedis.Jedis;

import java.util.Collections;
import java.util.List;

class WarmUp {
    private static final int TRY_UPPER_BOUND = 100000;

    static List<KV> warmUp(int totalLen) {
        List<KV> list = ListFactory.create(totalLen);
        int maxTry = 0;

        try (Jedis jedis = JedisUtils.acquire()) {
            jedis.flushDB();
            for (KV item : list) {
                int tryCount = 0;
                while (tryCount < TRY_UPPER_BOUND) {
                    try {
                        jedis.set(item.key, item.val);
                        break;
                    } catch (redis.clients.jedis.exceptions.JedisDataException eRedis) {
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
        }
        return list;
    }
}
