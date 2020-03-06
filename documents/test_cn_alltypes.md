[回中文总目录](menu_cn.md) 

# [返回测试目录](test_cn.md)

## 测试所有数据结构, 包括：String/List/Set/Hash/Zset/Geo/HyperLogLog/Stream

### 所用方法

```
def _warm_up_with_all_data_types()
_check_all_key_in_data_types()
```

### 如何运行
1. 启动RedRock
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
2. Python3下运行
```
_warm_up_with_all_data_types()
_check_all_key_in_data_types()
```
将会测试Redis下面的数据类型
* String
* List
* Set
* Hast
* Zset
* Geo
* HyperLogLog
* Stream

在testredrock目录下查看test_redrock.py了解更多.