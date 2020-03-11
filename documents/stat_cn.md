[回中文总目录](menu_cn.md)

# 统计

## Redis原有的统计

应该都支持，你可以参考：

info: https://redis.io/commands/info

memory https://redis.io/commands/memory-stats

slowlog https://redis.io/commands/slowlog

NOTE: slowlog可能因为读盘变得意义不大

## RedRock新增的统计

用redis-cli连入RedRock，然后在redis-cli窗口执行
```
rock report
```
在服务器端窗口，你可以看到有多少键在磁盘以及比例。

注意：这个命令是全键扫描，因此需要点时间。