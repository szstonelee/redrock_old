[Back Top Menu](../README.md)

# Supported Commands

## Collection of Redis Commands

Redis has big collection of commands. Please reference: https://redis.io/commands

I have not found any command RedRock not support. If you found, please let me know. Thank you.

## Commands not need to read disk though its value in disk
For example, the command 'Set'
When we issue:
```
set k1, new_val
```
Maybe k1's old value is in disk, but we do not care 
because after the update, every one only care the new value. 

Check the command list
* set
* setnx
* setex
* psetex
* del
* unlink
* exists
* mset
* msetnx
* randomkey
* expire
* expireat
* pexpire
* pexpireat
* keys
* scan
* multi
* discard
* ttl
* touch
* pttl
* persist
* watch
* unwatch
* restore
* restore-asking
* asking

## Commands has nothing related to value
Some commands, do not be related to value. So these commands, are free of reading disk:
* module
* select
* swapdb
* dbsize
* auth
* ping
* echo
* shutdown
* lastsave
* replconf
* flushdb
* flushall
* info
* monitor
* slaveof
* replicaof
* role
* config
* cluster
* readonly
* readwrite
* memory
* client
* hello
* slowlog
* script
* time
* wait
* command
* post
* host
* latency
* lolwut
* acl

## Commands relating to backup

Because backup means full value reading, these commands are absolutely related to disk.
We has to consider the consistency, because backup take a long time. Meanwhile, some keys could be in or out disk.

* save
* bgsave
* bgrewriteaof

Please reference [How backup and persist the memory data with the storage data.](persistence_en.md)

NOTE: the following commands is used for sync, something like backup

* sync
* psync

## Commands never dumping value to disk
For those commands, they use value. But the values never go to disk.
They are commands relating to stream and pub/sub.
* subscribe
* unsubscribe
* psubscribe
* punsubscribe
* publish
* pubsub
* xadd
* xrange
* xrevrange
* xlen
* xread
* xreadgroup
* xgroup
* xsetid
* xack
* xpending
* xclaim
* xinfo
* xdel
* xtrim

## Other Commands needs to read disk

The other commands, they maybe read value from disk.
Especially the following needed to be mentioned.

### Transacion: exec
When issue the transaction command, whether it needs to read disk is dependent on the commands in the transaction.
For example:
```
multi
set k1, v1
set k2, v2
exec
```

Because 'Set' command does not need read disk and the transaction only include commands like 'Set', 
the transaction does not need reading disk.

### Blocking Commands

When it come to the commands related to blocking, we always need to read all value from disk.

For example：

client A, try to block on two keys, k1，k2.
This time, k1 is empty, but k2's value is in disk, so it needs to read k2's value from disk.
Reading disk is slow. Meanwhile, another client B set k1.
Theorically, we can unblock Client A right now. 
But we DO NOT! It needs to read k2's value, then we unblock client A.

