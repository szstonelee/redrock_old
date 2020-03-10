[Back Top Menu](../README.md) 

# [Back To Test Cases](test_en.md)

## Test Script(Lua)

### The function for test

Check function
```
def _warm_up_with_string()
def _check_lua1()
def _check_lua2()
```

### How Test

#### Lua Test1

First, start RedRock as
MAC
```
./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
Linux
```
sudo ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
```
Second, in Python3, run
```
_warm_up_with_string()
_check_lua1()
```

#### Lua Test2

First, start RedRock as
MAC
```
./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
Linux
```
sudo ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
```
Second, in Python3, run
```
_warm_up_with_string()
_check_lua2()
```

Check the codes in test_redrock.py in testredrock folder for more details.