[Back Top Menu](../README.md) 

# [Back To Test Cases](test_en.md)

## Test Replication

1. start RedRock as
MAC
```
./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
Linux
```
sudo ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
```
2. in Python3, run
```
_warm_up_with_string()
```
3. run second RedRock server (if same machine, you need another listening port)
```
./redis-server --port 6380 --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --rockdbdir /opt/redrock_rocksdb2/ --save ""
```
4. redis-cli connect the second server
```
redis-cli -p 6380
then
replicaof 127.0.0.1 6379
```
5. check data in Python test script
When RedRock finish restoring the backup file
run python3 
```
_check_all_key_in_string()
```

Check the codes in test_redrock.py in testredrock folder for more details.