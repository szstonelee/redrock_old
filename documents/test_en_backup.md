[Back Top Menu](../README.md) 

# [Back To Test Cases](test_en.md)

## Test RDB or AOF backup

1. start RedRock as
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
2. in Python3, run
```
_warm_up_with_string()
```
3. save file
```
./redis-cli
then BGSAVE, SAVE or BGREWRITEAOF
```
4. stop RedRock
```
ctrl-C, to stop RedRock
```
5. start Redis-server or RedRock to load the backup file
```
./redis-server
or
./redis-server --appendonly yes
or
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes
or
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --appendonly yes
```
6. check data in Python test script
When RedRock finish restoring the backup file
run python3 
```
_check_all_key_in_string()
```

Check the codes in test_redrock.py in testredrock folder for more details.