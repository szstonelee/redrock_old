package metric;

import redis.clients.jedis.Jedis;

class MillionKeysFactory {
    private static final int TRY_UPPER_BOUND = 100;

    static MillionKeys warmUp(int howManyMillion) {

        MillionKeys millionKeys = new MillionKeys(howManyMillion);

        try (Jedis jedis = JedisUtils.acquire()) {
            jedis.flushDB();
            Thread.sleep(5000);

            int count = 0;
            while (millionKeys.hasNext()) {
                String key = millionKeys.next();
                String val = millionKeys.randValue();
                int ttl =  millionKeys.randTtl();

                if ((count % 100000) == 0) {
                    System.out.println("Warm up count = " + count + ", timestamp = " + System.currentTimeMillis()/1000);
                }

                int tryCount = 0;
                while (tryCount < TRY_UPPER_BOUND) {
                    try {
                        jedis.setex(key, ttl, val);
                        ++count;
                        break;
                    } catch (redis.clients.jedis.exceptions.JedisDataException e) {
                        ++tryCount;
                    }
                }
                if (tryCount >= TRY_UPPER_BOUND) {
                    System.out.println("Warm up try times warning!");
                    Thread.sleep(1000);
                }
            }
        } catch (InterruptedException e) {
        }

        return millionKeys;
    }

}
