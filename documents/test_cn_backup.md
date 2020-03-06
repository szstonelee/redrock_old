[回中文总目录](menu_cn.md) 

# [测试总目录](test_cn.md)

## 测试备份RDB或AOF

1. 像如下启动RedRock
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
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
./redis-server
or
./redis-server --appendonly yes
or
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes
or
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --appendonly yes
```
6. 用Python脚本验证数据完整性
python3 
```
_check_all_key_in_string()
```

在testredrock目录下查看test_redrock.py了解更多.