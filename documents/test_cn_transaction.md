[回中文总目录](menu_cn.md) 

# [返回测试目录](test_cn.md)

## 测试Transaction

### 所用方法

```
def _warm_up_with_string()
def _check_transaction()
```

### 如何运行
1. 启动RedRock
```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save ""
```
2. Python3下运行
```
_warm_up_with_string()
_check_transaction()
```

在testredrock目录下查看test_redrock.py了解更多.