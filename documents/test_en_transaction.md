[Back Top Menu](../README.md) 

# [Back To Test Cases](test_en.md)

## Test Transaction

### The function for test

Check function
```
def _warm_up_with_string()
def _check_transaction()
```

### How Test
First, start RedRock as
```
sudo ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
```
Second, in Python3, run
```
_warm_up_with_string()
_check_transaction()
```

Check the codes in test_redrock.py in testredrock folder for more details.