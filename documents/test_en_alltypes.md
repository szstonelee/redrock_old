[Back Top Menu](../README.md) 

# [Back To Test Cases](test_en.md)

## Test All Data Types, String/List/Set/Hash/Zset/Geo/HyperLogLog/Stream

### The function for test

Check function
```
def _warm_up_with_all_data_types()
_check_all_key_in_data_types()
```

### How Test
First, start RedRock as
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
Second, in Python3, run
```
_warm_up_with_all_data_types()
_check_all_key_in_data_types()
```
It will check the following data types of Redis
* String
* List
* Set
* Hast
* Zset
* Geo
* HyperLogLog
* Stream

Check the codes in test_redrock.py in testredrock folder for more details.