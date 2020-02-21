import redis
import time
import threading
import random

POOL = redis.ConnectionPool(host='127.0.0.1',
                            port='6379',
                            db=0,
                            decode_responses=True,
                            encoding='utf-8',
                            socket_connect_timeout=2)


def _test():
    r = redis.StrictRedis(connection_pool=POOL)
    res = r.client_setname('debug')
    print('ready for pipe')
    pipe = r.pipeline(transaction=False)
    pipe.get('abc')
    pipe.get('def')
    pipe.get('xxx')
    for i in range(1<<20):
        pipe.get('defgafarerwefdsfewrqwsafaerewrwer4dsfasdfsrffrerwqerqw')
    print('before pipe execute')
    res = pipe.execute()
    len_res = len(res)
    print(f'len = {len(res)}')
    for i in range(min(10, len_res)):
        print(res[i])


def _resume():
    r = redis.StrictRedis(connection_pool=POOL)
    time.sleep(2)
    r.echo('resume')


def _pipe():
    r = redis.StrictRedis(connection_pool=POOL)
    res = r.client_setname('debug')
    pipe = r.pipeline(transaction=False)
    pipe.get('abc')
    pipe.get('def')
    pipe.get('xxx')
    for _ in range(1<<15):
        pipe.get('defgafarerwefdsfewrqwsafaerewrwer4dsfasdfsrffrerwqerqw')
    t_resume = threading.Thread(target=_resume)
    t_resume.start()
    res = pipe.execute()
    t_resume.join()
    print(len(res))


def _leak():
    for _ in range(100000000):
        t_pipe = threading.Thread(target=_pipe)
        t_pipe.start()
        t_pipe.join()


def _inc_maxmemory_with_str():
    # ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()
    val = ''
    for i in range(2000):
        val += str(i)
    i = 0
    start = time.time()
    while i < 100000:
        try:
            r.set(i, val)
            i += 1
        except redis.exceptions.ResponseError:
            time.sleep(0.001)
            if (i % 10) == 0:
                print(f'exception i = {i}')
    end = time.time()
    print(f'duration = {int(end-start)} seconds')
    #r.bgsave()


def _check_all_key_in_str():
    # after execute _inc_maxmemory_with_str(), run this
    r = redis.StrictRedis(connection_pool=POOL)
    val = ''
    for i in range(2000):
        val += str(i)

    start = time.time()
    for i in range(100000):
        db_val = r.get(str(i))
        if db_val is None:
            print(f'None until {i}')
            break
        if db_val != val:
            print(f'wrong value, key = {i}, db val = {db_val}')
    end = time.time()
    print(f'latency = {int(end-start)} seconds, avg = {int(100000/(end-start))} rps')


def _dec_memory():
    total_latency = time.perf_counter()
    r = redis.StrictRedis(connection_pool=POOL)
    for _ in range(1):
        for i in range(100000):
            r.get(str(i))

    total_latency = time.perf_counter() - total_latency
    print(f'total latency = {total_latency}')


def _del_all_keys():
    total_latency = time.perf_counter()
    r = redis.StrictRedis(connection_pool=POOL)
    for _ in range(1):
        for i in range(1000000):
            r.delete(str(i))

    total_latency = time.perf_counter() - total_latency
    print(f'total latency = {total_latency}')


def _test_replica():
    r = redis.StrictRedis(connection_pool=POOL)
    count = 0
    for i in range(100000):
        val = r.get(str(i))
        if val is not None:
            if len(val) != 6890:
                print(f'key = {i}, val = {val}, len = {len(val)}')
                count += 1

    print(f'count = {count}')


def _test_ser_des_string():
    # ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()

    total = 300000
    val0 = 'aaa'
    val1 = 12
    val2 = 'b'*1000

    for i in range(total):
        m = i % 3
        if m == 0:
            r.set(i, val0)
        elif m == 1:
            r.set(i, val1)
        elif m == 2:
            r.set(i, val2)

    for i in range(total):
        val = r.get(i)
        m = i % 3
        if m == 0:
            if val != val0:
                print(f'wrong {i}, val = {val}')
        elif m == 1:
            if val != str(val1):
                print(f'wrong {i}, val = {val}')
        else:
            if val != val2:
                print(f'wrong {i}, val = {val}')


def _test_ser_des_list():
    # ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()

    total = 500000
    val0 = ['hello']
    val0_str = ['hello']
    val1 = ['a', 'bb', 'c'*1000]
    val1_str = val1
    val2 = [50, 'ddd', 999999]
    val2_str = ['50', 'ddd', '999999']

    #val1 = val0
    #val1_str = val0_str
    #val2 = val0
    #val2_str = val0_str

    for i in range(total):
        m = i % 3
        if m == 0:
            val = val0
        elif i % 3 == 1:
            val = val1
        else:
            val = val2

        r.rpush(i, *val)

    #return

    for i in range(total):
        val = []
        while True:
            item = r.lpop(str(i))
            if item is None:
                break
            else:
                val.append(item)
        m = i % 3
        if m == 0:
            if val != val0_str:
                print(f'wrong {i}, val = {val}')
        elif m == 1:
            if val != val1_str:
                print(f'wrong {i}, val = {val}')
        else:
            if val != val2_str:
                print(f'wrong {i}, val = {val}')


def _inc_maxmemory_with_list():
    # ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()

    total = 10000

    val = []
    start = time.time()
    for i in range(total):
        val.append(i)
        r.rpush(i, *val)
    end = time.time()
    print(f'duration = {int(end-start)} seconds')


def _test_block_list():
    # first run _inc_maxmemory_with_list()
    r = redis.StrictRedis(connection_pool=POOL)
    total = 10000

    for i in range(total):
        val = r.brpop(i, 1)
        if val is None:
            print(f'time out for {i}')
        elif val[1] != str(i):
            print(f'wrong for {i}, val = {val}')


def _inc_maxmemory_with_intset():
    # ./redis-server --maxmemory 50m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()

    total = 30000

    start = time.time()
    for i in range(total):
        for j in range(32):
            r.sadd(i, j)
    end = time.time()
    print(f'duration = {int(end-start)} seconds')

    v = 'a' * 10000
    for i in range(total, total+3000):
        r.set(i, v)

    for i in range(total):
        rand = random.randint(0, 31)
        exist = r.sismember(i, rand)
        if not exist:
            print(f'wrong for {i}')


def _inc_maxmemory_with_ht():
    # ./redis-server --maxmemory 50m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()

    total = 3000

    start = time.time()
    for i in range(total):
        for j in range(32):
            val = 'a' * j
            r.sadd(i, val)
    end = time.time()
    print(f'duration = {int(end-start)} seconds')

    v = 'a' * 10000
    for i in range(total, total+3000):
        r.set(i, v)

    for i in range(total):
        rand = random.randint(0, 31)
        rand_val = 'a' * rand
        exist = r.sismember(i, rand_val)
        if not exist:
            print(f'wrong for {i}')


def _test_ht_with_init_one_specific_key():
    # reference _test_ser_des_hash_ht()
    key = 'abc'
    r = redis.StrictRedis(connection_pool=POOL)

    for i in range(10000):
        r.hset(key, i, i)


def _test_zset_with_init_one_specific_key():
    # reference _test_ser_des_zset_skiplist()
    key = 'abc'
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()

    score = 0.123
    for i in range(1000):
        r.zadd(key, {"field+"+str(i): score})
        score += 1


def _test_hash():
    # ./redis-server --maxmemory 50m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()

    total = 1000
    for i in range(total):
        if i % 2 == 0:
            r.hset(i, 0, 123)
            r.hset(i, 1, 'abc')
            r.hset(i, 2, 456)
            r.hset(i, 3, 'defg')
        else:
            for j in range(1000):
                r.hset(i, j, 'a'*j)

    for i in range(total):
        if i % 2 == 0:
            val = r.hget(i, 2)
            if val != '456':
                print(f'wrong, ziplist, {i}, val = {val}')
        else:
            val = r.hget(i, 555)
            if val != 'a'*555:
                print(f'wrong, ht, {i}, val = {val}')


def _test_zset():
    # ./redis-server --maxmemory 50m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()

    total = 1000
    for i in range(total):
        if i % 2 == 0:
            r.zadd(i, {"abc": 10})
            r.zadd(i, {"defg": 22})
            r.zadd(i, {"888": 88})
        else:
            score = 0.1234
            for j in range(1000):
                r.zadd(i, {'skiplist+'+str(j): score})
                score += 1

    for i in range(total):
        if i % 2 == 0:
            val = r.zpopmax(i)
            if val[0] != '888' or val[1] != 88.0:
                print(f'wrong, zset, ziplist, {i}, val = {val}')
        else:
            val = r.zrank(i, 'skiplist+5')
            if val != 5:
                print(f'wrong, ht, {i}, val = {val}')


if __name__ == '__main__':
    #_inc_maxmemory_with_str()
    _check_all_key_in_str()

    #_test()
    #_leak()
    #_dec_memory()
    #_del_all_keys()
    #_test_replica()
    #_test_ser_des_string()
    #_test_ser_des_list()
    #_inc_maxmemory_with_list()
    #_test_block_list()
    #_inc_maxmemory_with_intset()
    #_inc_maxmemory_with_ht()
    #_test_ht_with_init_one_specific_key()
    #_test_hash()
    #_test_zset_with_init_one_specific_key()
    #_test_zset()
