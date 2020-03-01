import redis
import time
import threading
import random
import threading

POOL = redis.ConnectionPool(host='127.0.0.1',
                            port='6379',
                            db=0,
                            decode_responses=True,
                            encoding='utf-8',
                            socket_connect_timeout=2)

LIST_VAL_LEN = 100
SET_VAL_LEN = 1000
HASH_VAL_LEN = 1000
ZSET_VAL_LEN = 100

# ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
# or
# ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --save ""
# after success, run redis-cli, issue command 'rock keyreport'
# if you see how many keys in disk, you can check one rock key in the report by command 'get'
# the value for it is like '01234567....'
# try using 1, 2, 3, 4 million for the two situaations
# when error exception: OOM command not allowed when used memory > 'maxmemory'
def _warm_up_with_string(max_keys: int = 1_000_000):
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()

    val = ''
    for i in range(2_000):
        val += str(i)
    print(f'value length = {len(val)}')

    start = time.time()
    for i in range(max_keys):
        if i % 100_000 == 0:
            print(f"i = {i}, at time = {int(time.time())}")
        r.set(i, val)

    end = time.time()
    print(f'Success! Warm up for total keys = {max_keys}, duration = {int(end-start)} seconds')


# after _warm_up_with_string, using this to check all key's value even with most value in rocksdb
def _check_all_key_in_string(max_keys: int = 1_000_000):
    r = redis.StrictRedis(connection_pool=POOL)
    val = ''
    for i in range(2_000):
        val += str(i)

    start = time.time()
    for i in range(max_keys):
        if i % 100_000 == 0:
            print(f'i = {i}, time = {int(time.time())}')
        db_val = r.get(str(i))
        if db_val is None:
            print(f'None until {i}')
            return
        if db_val != val:
            print(f'wrong value, key = {i}, db val = {db_val}')
            return
    end = time.time()
    print(f'Success! all keys value check correct!!! latency = {int(end-start)} seconds, avg = {int(100000/(end-start))} rps')


# include every datatype
# string: 0123...1999 string value
# list: 0,1,100 list
# set: 0,1,1000
# hashset: field: 0, 1000, val: 0123..99
# Zset: score 0-999, field 0-999

def _warm_up_with_all_data_types(max_keys: int = 3_000):
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()

    string_val = ''
    for i in range(2_000):
        string_val += str(i)
    hash_field_val = ''
    for i in range(100):
        hash_field_val += str(i)

    start = time.time()
    for i in range(max_keys):
        if i % 1000 == 0:
            print(f"i = {i}, at time = {int(time.time())}")

        data_type = i % 8
        if data_type is 0:
            # String
            r.set(i, string_val)
        elif data_type is 1:
            # list
            for j in range(LIST_VAL_LEN):
                r.lpush(i, j)
        elif data_type is 2:
            # set
            for j in range(SET_VAL_LEN):
                r.sadd(i, j)
        elif data_type is 3:
            # hash
            for j in range(HASH_VAL_LEN):
                r.hset(i, j, hash_field_val)
        elif data_type is 4:
            # zset
            for j in range(ZSET_VAL_LEN):
                r.zadd(i, {j:j})
        elif data_type is 5:
            # Geo
            r.geoadd(i, 13.361389, 38.115556, "Palermo", 15.087269, 37.502669, "Catania")
        elif data_type is 6:
            # HyperLogLog
            r.pfadd(i, 'a', 'b', 'c', 'd', 'e', 'f', 'g')
        elif data_type is 7:
            # Stream
            r.xadd(i, fields={'field1':'value1', 'field2':'value2'})
        else:
            raise AssertionError

    end = time.time()
    print(f'Success! Warm up for total keys = {max_keys}, duration = {int(end-start)} seconds')


def _check_all_key_in_data_types(max_keys: int = 3_000):

    r = redis.StrictRedis(connection_pool=POOL)
    string_check = ''
    for i in range(2_000):
        string_check += str(i)
    list_check = []
    for i in range(LIST_VAL_LEN):
        list_check.append(str(i))
    list_check.sort()
    set_check = set()
    for i in range(SET_VAL_LEN):
        set_check.add(str(i))
    hash_field_val = ''
    for i in range(100):
        hash_field_val += str(i)
    hash_check = dict()
    for i in range(HASH_VAL_LEN):
        hash_check[str(i)] = hash_field_val
    zset_check = []
    for i in range(ZSET_VAL_LEN):
        zset_check.append((str(i), float(i)))

    start = time.time()
    for i in range(max_keys):
        if i % 1_000 == 0:
            print(f"i = {i}, at time = {int(time.time())}")

        data_type = i % 8
        if data_type is 0:
            # String
            val = r.get(i)
            if val is None:
                raise Exception("None for string")
            if val != string_check:
                raise Exception("String value wrong")
        elif data_type is 1:
            # list
            val = r.lrange(i, 0, -1)
            if val is None:
                raise Exception("None for list")
            val.sort()
            if val != list_check:
                raise Exception("List value wrong")
        elif data_type is 2:
            # set
            val = r.smembers(i)
            if val is None:
                raise Exception("None for set")
            if val != set_check:
                raise Exception("Set value wrong")
        elif data_type is 3:
            # hash
            val = r.hgetall(i)
            if val is None:
                raise Exception("None for hash")
            if val != hash_check:
                raise Exception("Hash value wrong")
        elif data_type is 4:
            # zset
            val = r.zrange(i, 0, -1, withscores=True)
            if val is None:
                raise Exception("None for hash")
            if val != zset_check:
                raise Exception("Zset value wrong")
        elif data_type is 5:
            # Geo
            val = r.geohash(i, 'Catania', 'Palermo')
            if val is None:
                raise Exception("None for Geo")
            if val != ['sqdtr74hyu0', 'sqc8b49rny0']:
                raise Exception("Geo value wrong")
        elif data_type is 6:
            # HyperLogLog
            val = r.pfcount(i)
            if val is None:
                raise Exception("None for HyperLogLog")
            if val != 7:
                raise Exception("HyperLogLog value wrong")
        elif data_type is 7:
            # Stream
            val = r.xrange(i)
            if val is None:
                raise Exception("None for Stream")
            if val[0][1] != {'field1':'value1', 'field2':'value2'}:
                raise Exception("HyperLogLog value wrong")
        else:
            raise AssertionError

    end = time.time()
    print(f'Success! all keys value check correct!!! latency = {int(end-start)} seconds, avg = {int(100000/(end-start))} rps')


# please run _warm_up_with_string() first
def _check_pipeline(max_keys: int = 1_000_000):
    r = redis.StrictRedis(connection_pool=POOL)
    val = ''
    for i in range(2_000):
        val += str(i)

    for _ in range(1_000):
        with r.pipeline(transaction=False) as pipe:
            for _ in range(100):
                random_key = random.randint(0, max_keys-1)
                pipe.get(random_key)
            batch = pipe.execute()
            for i in range(100):
                if batch is None or batch[i] != val:
                    raise Exception("pipeline return wrong values")

    print("Success for pipeline!")


# please run _warm_up_with_string() first
def _check_transaction(max_keys: int = 1_000_000):
    r = redis.StrictRedis(connection_pool=POOL)
    basic_val = ''
    for i in range(2_000):
        basic_val += str(i)
    basic_len = len(basic_val)

    for i in range(10_000):
        if i % 1_000 == 0:
            print(f"transction i = {i}, at time = {int(time.time())}")

        with r.pipeline(transaction=True) as pipe:
            random_key1 = random.randint(0, max_keys-1)
            pipe.get(random_key1)
            random_key2 = random.randint(0, max_keys-1)
            pipe.get(random_key2)
            pipe.append(random_key2, basic_val)
            pipe.execute()

    multi_count = 0
    for i in range(max_keys):
        if i % 10_000 == 0:
            print(f"transaction check i = {i}, at time = {int(time.time())}")

        val = r.get(i)
        multi = int(len(val)/basic_len)
        if multi != 1:
            multi_count += 1
        if val != basic_val*multi:
            raise Exception("transaction error")

    print(f"Success for transaction! multi count = {multi_count}")


def _warm_up_for_block(max_keys: int = 50_000):
    r = redis.StrictRedis(connection_pool=POOL)
    r.flushall()
    
    string_val = ''
    for i in range(2_000):
        string_val += str(i)
    list_val = []
    for i in range(1_000):
        list_val.append(i)

    for i in range(max_keys):
        if i % 1_000 == 0:
            print(f'i = {i}, time = {int(time.time())}')
        is_list = (i % 2 == 0)

        if is_list:
            r.rpush(i, *list_val)
        else:
            r.set(i, string_val)


# run _warm_up_for_block() first
def _check_block(max_keys: int = 50_000):
    r = redis.StrictRedis(connection_pool=POOL)

    for i in range(max_keys):
        if i % 1_000 == 0:
            print(f'i = {i}, time = {int(time.time())}')
        is_list = (i % 2 == 0)

        if is_list:
            val = r.blpop(i)
            if val is None or val != (str(i), '0'):
                raise Exception(f"error for blpop, val = {val}")
    
    print(f"Success for block!")


def _check_rdb():
    # first, run _warm_up_with_string() with
    # ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    # then redis-cli, bgsave, it takes time (you need check the save success result, i.e. "DB saved on disk"
    # and you can see two process named as redis-server, one with huge memory
    # theh, ctrl-c (double)
    # then, ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes
    # then _check_all_key_in_string()
    # repeat above but use save (not fork())
    pass


def _check_replication():
    # run first redis-server,
    # ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
    # run _warm_up_with_string() to ingest data to the first redis-server
    # run second redis-server (if same machine like me, use different port and different folder, check the follwing)
    # ./redis-server --port 6380 --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --rockdbdir /opt/redrock_rocksdb2/ --save ""
    # redis-cli -p 6380 to connect to the second redis-server, use replicaof 127.0.0.1 6379 command
    # it will take a long time ...
    # use _check_all_key_in_string() but change the redis POOL config to use second port 6380 (you can test first 6379 also)
    pass


# run _warm_up_with_string() first
def _check_lua1(max_keys: int = 1_000_000):
    r = redis.StrictRedis(connection_pool=POOL)

    check_val = ''
    for i in range(2_000):
        check_val += str(i)

    script = """
    local val = redis.call('GET', KEYS[1])
    return val 
    """
    func = r.register_script(script)

    for _ in range(10_000):
        rand_key = str(random.randint(0, max_keys-1))
        res = func(keys=[rand_key])
        if res != check_val:
            raise Exception("val not correct!")


# run _warm_up_with_string() first
def _check_lua2(max_keys: int = 1_000_000):
    r = redis.StrictRedis(connection_pool=POOL)

    check_val = ''
    for i in range(2_000):
        check_val += str(i)

    script = """
    local vals = {}
    local total = ARGV[1]
    for i = 1, total, 1
    do    
        local val = redis.call('GET', KEYS[i])
        vals[i] = val
    end 
    return vals 
    """
    lua_func = r.register_script(script)

    TOTAL_THREAD_NUMBER = 10
    thread_return_strings = []
    for _ in range(TOTAL_THREAD_NUMBER):
        thread_return_strings.append('')

    def thread_func(tid: int, key: str):
        r_thread = redis.StrictRedis(connection_pool=POOL)
        thread_val = r_thread.get(key)
        if thread_val is None:
            raise Exception("thread failed for Nil")
        thread_return_strings[tid] = thread_val

    for _ in range(1000):
        random_keys = []
        for _ in range(TOTAL_THREAD_NUMBER):
            random_keys.append(str(random.randint(0, max_keys-1)))
        ts = []
        for i in range(TOTAL_THREAD_NUMBER):
            t = threading.Thread(target=thread_func, args=(i, random_keys[i],))
            ts.append(t)
            t.start()
        lua_return_strings = lua_func(keys=random_keys, args=[TOTAL_THREAD_NUMBER])
        for i in range(TOTAL_THREAD_NUMBER):
            ts[i].join()
        # check the value
        for i in range(TOTAL_THREAD_NUMBER):
            if lua_return_strings[i] != check_val:
                raise Exception("lua return value not correct!")
            if thread_return_strings[i] != check_val:
                raise Exception("thread value not correct!")
            


# _warm_up_with_string() and _warm_up_with_string(false)
def _main():
    #_warm_up_with_string()
    _check_all_key_in_string()
    #_warm_up_with_all_data_types()
    #_check_all_key_in_data_types()
    #_check_pipeline()
    #_warm_up_for_block()
    #_check_block()
    #_check_transaction()
    #_check_lua1()
    #_check_lua2()
    pass


if __name__ == '__main__':
    _main()
