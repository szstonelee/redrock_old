[Back Top Menu](../README.md)

# Stats

## The original Redis Stats

I think they should be supported. Reference as list:

info: https://redis.io/commands/info

memory https://redis.io/commands/memory-stats

slowlog https://redis.io/commands/slowlog

NOTE: Because the latency of disk access, slowlog is not that meaningful.

## New Stats for RedRock

in redis-cli, or any redis client, issue the following command
```
rock report
```
From the server console, you can get how many and percentage of the keys' value in disk.

NOTE: The command will scan all keys, but no access to disk. It will take a little while.