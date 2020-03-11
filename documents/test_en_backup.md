[Back Top Menu](../README.md) 

# [Back To Test Cases](test_en.md)

## NOTE: overcommit_memory == 0

Some times, in your OS, overcommit_memory == 0.

When you run RedRock (or pure Redis), you will see the warning from the terminal window
```
WARNING overcommit_memory is set to 0! Background save may fail under low memory condition. To fix this issue add 'vm.overcommit_memory = 1' to /etc/sysctl.conf and then reboot or run the command 'sysctl vm.overcommit_memory=1' for this to take effect.
```
You should set overcommit_memory to 1.

## Test RDB or AOF backup

1. start RedRock as
```
sudo ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
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
sudo ./redis-server --bind 0.0.0.0
or
sudo ./redis-server --appendonly yes --bind 0.0.0.0
or
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --bind 0.0.0.0
or
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --appendonly yes --bind 0.0.0.0
```
6. check data in Python test script
When RedRock finish restoring the backup file
run python3 
```
_check_all_key_in_string()
```

Check the codes in test_redrock.py in testredrock folder for more details.