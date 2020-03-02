[Back Top Menu](../README.md)

# Supported Commands

## Collection of Redis Commands

Redis has a big collection of commands. Please reference: https://redis.io/commands

I think RedRock support most commands like Redis. 

If you find one Redis command RedRock does not support, please let me know. Thank you.

## Commands no need to access disk though maybe its value in disk
For example, the command 'Set'
```
set k1, new_value
```
Maybe k1's old value is in disk, but we do not care 

because after the update, every one only care about the new_value. 

So thease commands run as fast as in memory with its value in disk.

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
* pttl
* persist
* watch
* unwatch
* restore
* restore-asking
* asking

## Commands has nothing related to value
Some commands, do not need value. 

So these commands, are free of accessing disk:
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

We have to consider the consistency, because backup take a long time. During backup, some keys may be changed.

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

## Other Commands needs to access disk

The other commands, they maybe access value from disk.

Especially the following commands which need to pay attention to:

### Transacion: exec
When issue the transaction command, whether it needs to access disk is dependent on the commands in the transaction.

For example:
```
multi
set k1, v1
set k2, v2
exec
```

Because 'Set' command does not need to access disk and the transaction only include commands like 'Set', 
the transaction does not need to access disk at all.

### Blocking Commands

When it comes to the commands related to blocking, we always need to read all value from disk.

For example：

client A, try to block on two keys, k1，k2.
This time, k1 is empty, but k2's value is in disk, so it needs to read k2's value from disk.
Reading disk is slow. Meanwhile, another client B set k1.
Theorically, we could unblock Client A right now. 
But RedRock DO NOT! It needs to read k2's value from disk, after that RedRock unblocks client A.

### Script(LUA) commands

Such as EVAL.

Because Redis execute a script with no interruption, 

it maybe has a lot of Redis commands which need to access disk, 

just like the transaction way. 

So take care of Script commands just like taking care of Transaction commands. If the whole script or whole transaction needs to do a lot of commands relatiing to disk, it could execute very slow and slow down other client connections.

