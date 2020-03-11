[回中文总目录](menu_cn.md) 

# [测试总目录](test_cn.md)

## 测试LFU

### 方法名

```
def _warm_lfu_for_eviction_check()
def _check_lfu_for_eviction()
```

### 如何运行
首先，启动RedRock
```
sudo ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --save "" --maxmemory-policy allkeys-lfu --bind 0.0.0.0
```
在Python3下，运行
```
_warm_lfu_for_eviction_check()
_check_lfu_for_eviction()
```

在testredrock目录下查看test_redrock.py了解更多.