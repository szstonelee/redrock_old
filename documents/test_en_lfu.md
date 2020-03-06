[Back Top Menu](../README.md) 

# [Back To Test Cases](test_en.md)

## Test LFU

### The function for test

Check function
```
def _warm_lfu_for_eviction_check()
def _check_lfu_for_eviction()
```

### How Test
First, start RedRock as
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --save "" --maxmemory-policy allkeys-lfu
```
Second, in Python3, run
```
_warm_lfu_for_eviction_check()
_check_lfu_for_eviction()
```

Check the codes in test_redrock.py in testredrock folder for more details.