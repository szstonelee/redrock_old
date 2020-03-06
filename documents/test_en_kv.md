[Back Top Menu](../README.md) 

# [Back To Test Cases](test_en.md)

## Test String Key/Value

### The function for test

Check function
```
def _warm_up_with_string()
```

### First Test Case for K/V: One Million Key/value with maxmemory-only-for-rocksdb == yes

Let check when maxmemory-only-for-rocksdb == yes and 100MB memory, we insert one million key/value 
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
Run _warm_up_with_string(1_000_000) 

After finish, you can check the '[rockdbdir](howrun_en.md)' folder which default value is '/opt/redrock_rocksdb/'
```
du -h /opt/redrock_rocksdb
```
You can find serveral G volume in disk. This means values in Rocksdb. It is a good sign!

We want to know how many keys' value in disk. We can use redis-cli to connect RedRock, then
```
./redis-cli
rock report
```
You will something like this in RedRock terminal window
```
db=0, at lease one rock key = 794673
db=0, key total = 1000000, rock key = 994124, percentage = 99%, hot key = 5876, shared = 0, stream = 0
all db key total = 1000000, other total = 5876
```
The first line of the result is one rock key which means its value in disk. 

You can get the key to test its value. The value's pattern is like '01234......9999'.

'db=0, key total', is current keys in mememory, it is one million. it is Great!

There are 994124 which are rock keys (NOTE: for each machine the number is different), it means these keys' value in disk, the percentage is 99%.

Hot key is keys with value in memory but is not shared key which means it could be dumped to disk in future.

Shared key is some common string, like string '1', '2'. It is like Java interning string or Python Integer of -128~127. These value are common value and usually shared by some keys (for save memory) and can not be dumped to disk. And the number of this kind of value is usually very small, so it does not have big impact for the memory. So you just know it but do not care much about it.

Steam key is not dumped to disk, i.e. stream key is not rock key and would not be in hot key pool.

Other total usually means hot keys but you do not to care about the number.

### Second Test Case for K/V: Four Millions Keys with maxmemory-only-for-rocksdb == yes

If we run 
```
_warm_up_with_string(4_000_000)
```
We want to insert 4 million keys to RedRock in 100 MB memory (it is **impossible**!), what will happen?

You will see the wrong response (exception) from the python tool
```
redis.exceptions.ResponseError: OOM command not allowed when used memory > 'maxmemory'.
```

That is correct!!! Because we can not accomdate 4 million keys in 100MB memory. 

RedRock denies service just like Redis does the same way.

We use 
```
rock report
```
This time, the result is 
```
db=0, key total = 2058037, rock key = 2058037, percentage = 100%, hot key = 0, shared = 0, stream = 0
```

You see, we insert 2058037(NOTE:differnet machine has different number) keys in 100MB memory. 
And all these keys' value in disk, so the percentage is 100%.

### Third Test Case for K/V: One Million Keys with maxmemory-only-for-rocksdb == no

```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --save ""
```
this time, we run 
```
rock report
```
the result is similar as the first user case, one Million Keys with maxmemory-only-for-rocksdb == yes.

It is OK, because 99% keys' value in disk, but there are at 1% key can hold their value in memory.

### Fourth Test Case for K/V: Four Million Keys with maxmemory-only-for-rocksdb == no && enable eviction

```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --save "" --maxmemory-policy allkeys-random
```

run 
```
_warm_up_with_string(4_000_000)
```
This time, no exception! no error!

We check how many keys in memory by run 'rock report'

The result is like 
```
db=0, key total = 1907718, rock key = 1906765, percentage = 99%, hot key = 953, shared = 0, stream = 0
```
We inserted 4 milliion keys, almost half kept in memory, others are be evicted because we grant RedRock to do it (by --maxmemory-policy allkeys-random). That is why no OOM error or exception.

### Fifth Test Case for K/V: Insert one millions keys then read with value check

```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
Python run 
_warm_up_with_string(1_000_000)
_check_all_key_in_string()
```

You will see no error. Check the python codes for more details.




