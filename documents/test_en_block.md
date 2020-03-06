[Back Top Menu](../README.md) 

# [Back To Test Cases](test_en.md)

## Test Blocking

### The function for test

Check function
```
def _warm_up_for_block()
def _check_block()
```

### How Test
First, start RedRock as
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
Second, in Python3, run
```
_warm_up_for_block()
_check_block()
```

Check the codes in test_redrock.py in testredrock folder for more details.