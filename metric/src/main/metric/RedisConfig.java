package metric;

class RedisConfig {
    final String server = "127.0.0.1";
    final int port = 6379;
    final int database = 0;
    final int readTimeout = 50000;
    final int maxTotalConnections = 32;
    final int maxIdleConnections = 32;
    final int minIdleConnections = 0;
    final int poolMaxWait = 5000;
}
