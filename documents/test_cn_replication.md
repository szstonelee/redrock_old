[回中文总目录](menu_cn.md) 

# [返回测试目录](test_cn.md)

## 测试Replication

1. 启动RedRock
```
sudo ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
```
2. python3下
```
_warm_up_with_string()
```
3. 运行第二个RedRock (如果是同一台机器上，需要第二端口，如下)
```
sudo ./redis-server --port 6380 --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --rockdbdir /opt/redrock_rocksdb2/ --save ""  --bind 0.0.0.0
```
4. 用redis-cli连入第二个RedRock
```
redis-cli -p 6380
then
replicaof 127.0.0.1 6379
```
5. 当同步完成后，用下面的方法检查数据完整（注意：client端连接端口是6380） 
```
_check_all_key_in_string()
```

在testredrock目录下查看test_redrock.py了解更多.