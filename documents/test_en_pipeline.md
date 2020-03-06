[Back Top Menu](../README.md) 

# [Back To Test Cases](test_en.md)

## Test Pipeline

### The function for test

Check function
```
def _warm_up_with_string()
def _check_pipeline()
```

### How Test
First, start RedRock as
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
Second, in Python3, run
```
_warm_up_with_string()
_check_pipeline()
```

Check the codes in test_redrock.py in testredrock folder for more details.