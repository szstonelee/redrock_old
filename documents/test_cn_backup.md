[回中文总目录](menu_cn.md) 

# [测试总目录](test_cn.md)

## 注意：overcommit_memory == 0

有时你的系统里, overcommit_memory == 0.

检测方法：你运行RedRock (或者运行一个纯redis也可以), 你会发现下面这个警告
```
WARNING overcommit_memory is set to 0! Background save may fail under low memory condition. To fix this issue add 'vm.overcommit_memory = 1' to /etc/sysctl.conf and then reboot or run the command 'sysctl vm.overcommit_memory=1' for this to take effect.
```
这时，你应该设置overcommit_memory为1.

## 测试备份RDB或AOF

1. 像如下启动RedRock
```
sudo ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
```
2. Python3下运行
```
_warm_up_with_string()
```
3. 存盘
```
./redis-cli 连入
下面几个命令都是备份命令 BGSAVE, SAVE or BGREWRITEAOF
```
4. 停止RedRock
```
ctrl-C, 就可以停止RedRock
```
5. 再启动Redis-server或RedRock，同时让启动重新载入备份文件
```
sudo ./redis-server --bind 0.0.0.0
or
sudo ./redis-server --appendonly yes --bind 0.0.0.0
or
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --bind 0.0.0.0
or
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --appendonly yes --bind 0.0.0.0
```
6. 用Python脚本验证数据完整性
python3 
```
_check_all_key_in_string()
```

在testredrock目录下查看test_redrock.py了解更多.