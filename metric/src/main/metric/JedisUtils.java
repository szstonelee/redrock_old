package metric;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import redis.clients.jedis.Jedis;
import redis.clients.jedis.JedisPool;
import redis.clients.jedis.JedisPoolConfig;
import redis.clients.jedis.exceptions.JedisException;
import redis.clients.util.Pool;


class JedisUtils {
    private static Pool<Jedis> jedisPool;
    private static final Logger logger = LoggerFactory.getLogger(JedisUtils.class);

    private JedisUtils() {}

    private static void init() {
        RedisConfig config = new RedisConfig();
        JedisPoolConfig jedisConf = new JedisPoolConfig();

        jedisConf.setMaxTotal(config.maxTotalConnections);
        jedisConf.setMaxIdle(config.maxIdleConnections);
        jedisConf.setMinIdle(config.minIdleConnections);
        jedisConf.setMaxWaitMillis(config.poolMaxWait);

        jedisPool = new JedisPool(jedisConf, config.server, config.port,
                config.readTimeout, null, config.database);
    }

    public static Jedis acquire() {
        if (jedisPool == null) {
            init();
        }

        try {
            return jedisPool.getResource();
        } catch (JedisException e) {
            logger.error("redis client acquire failed, msg = {}, e = ", e.toString(), e);
            throw e;
        }
    }
}
