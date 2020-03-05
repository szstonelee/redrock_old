[Back Top Menu](../README.md)

# Test Cases

We can test all user cases for RedRock. So you can have the confidence that RedRock runs as you want.

## Pure Redis Tests

### How run tests for Pure Redis Tests

First and most, we need to know, whether RedRock can pass all Redis tests.

Redis include a test tool written by tcl. You need install tcl in your OS.

Then
```
cd src
make
make test
```
NOTE: when run make test, there should no redis-server is running, i.e. No 6379 port is being listened.

The test has  52 test user cases. It takes a long time for all tests, and you can have a cup of coffee.

### Linux Special Note for Transparent Huge Pages(THP)

Some Linux has a default feature -- Transparent Huge Pages.

If this feature is on in Linux, the 52th test can not pass.

#### Check whether THP on in your Linux 

You can check whether it is on in your Linux by two ways:

1. run redis-server (it could be RedRock)
```
./redis-server
```
In the terminal window, if you see Transparent Huge Pages warning, it means your Linux enable the feature.
2. check the file
```
sudo cat /sys/kernel/mm/redhat_transparent_huge
```
If you see something other than 'never', it means your Linxu enable THP.

#### How disable THP in your Linux (permanently)

Google!

My way:

```
sudo vi /etc/default/grub
add or modify the sentence:
RUB_CMDLINE_LINUX_DEFAULT="transparent_hugepage=never quiet splash"
then quit and save
then reboot
then check sudo cat /sys/kernel/mm/redhat_transparent_huge
```

## My test case tool

Please check the python script for the tool, 

it needs Python3 & redis-client for Python (e.g. https://github.com/andymccurdy/redis-py)
```
cd testredrock
ls test_redrock.py
```

## Test RedRock with Key/Value

### the function to use for test

Check function
```
def _warm_up_with_string()
```

### 1 Million Keys with maxmemory-only-for-rocksdb == yes

You need to check two conditions, first, --maxmemory-only-for-rocksdb yes
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
Run _warm_up_with_string() with inserting 1 Million Key/Value (the default parameter) but limited memory to 100MB.

After finish, you can check the '[rockdbdir](howrun_en.md)' folder which default value is '/opt/redrock_rocksdb/'
```
du -h /opt/redrock_rocksdb
```
You can serveral G volume in disk. This means values in Rocksdb. It is a good sign!

We want to know how many keys' value in disk. We can use redis-cli connecting RedRock, then
```
rock report
```
You will something like this in RedRock process terminal 
```
db=0, at lease one rock key = 794673
db=0, key total = 1000000, rock key = 994124, percentage = 99%, hot key = 5876, shared = 0, stream = 0
all db key total = 1000000, other total = 5876
```
The first line of the result is one rock key which means its value in disk. You can get the key to test its value. The value's pattern is like '01234......9999'.

db 0 total, is current keys in mememory, its 1 million, it is OK!

There are 994124 which are rock keys (NOTE: each machine the number is different), it means these keys' value in disk, the percentage is 99%.

Hot key is keys with value in memory but is not shared key which means it could be dumped to disk in future.

Shared key is some common string, like string '1', '2'. It is like Java interning string or Python common Integer. These value are common value and usually shared by some keys (for save memory) and can not be dumped to disk. And the number of this kind of value is very limited, so it does not have big impact for the memory. So you just know it but do not care much about it.

Steam key is not dumped to disk, i.e. stream key is not rock key and would be in hot key poool.

Other total usually means hot keys but you does not to care about it.

### 4 Millions Keys

If we run 
```
_warm_up_with_string(4_000_000)
```
We want to insert 4 million keys to RedRock in 100 MB memory (it is **impossible**!), what will happen?

You will see the wrong info from the python tool
```
redis.exceptions.ResponseError: OOM command not allowed when used memory > 'maxmemory'.
```

That is correct!!! Because we can not accomdate 4 million keys in 1MB memory. 

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

### 1 Million Keys with maxmemory-only-for-rocksdb == no

```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --save ""
```
this time, we run 
```
rock report
```
the result is similar as the 1 Million Keys with maxmemory-only-for-rocksdb == yes.

It is OK, because 99% keys' value in disk, but there are at 1% key can hold their value in memory.

### 4 Million Keys with maxmemory-only-for-rocksdb == no && enable eviction

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
We inserted 4 milliion keys, almost half kept in memory, others are be evicted because we let RedRock do it (by --maxmemory-policy allkeys-random). That is why no OOM error.



