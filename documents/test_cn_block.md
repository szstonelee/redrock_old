[回中文总目录](menu_cn.md) 

# [返回测试目录](test_cn.md)

## 测试Blocking

### 所用方法

```
def _warm_up_for_block()
def _check_block()
```

### 如何运行
1. 启动RedRock
```
sudo ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
```
2. Python3
```
_warm_up_for_block()
_check_block()
```

在testredrock目录下查看test_redrock.py了解更多.