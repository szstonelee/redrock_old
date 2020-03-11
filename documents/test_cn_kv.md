[回中文总目录](menu_cn.md) 

# [返回测试目录](test_cn.md)

## 测试String Key/Value

### 所用方法

```
def _warm_up_with_string()
```

### 第一个测试用例：K/V: 一百万 maxmemory-only-for-rocksdb == yes

在maxmemory-only-for-rocksdb == yes以及内存限制在100MB下, 我们尝试插入一百万键和值 
```
sudo ./redis-server --maxmemory 200m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
```
运行_warm_up_with_string(1_000_000) 

结束后，你检查配置参数'[rockdbdir](howrun_cn.md)' 目录，其缺省值是'/opt/redrock_rocksdb/'
```
du -h /opt/redrock_rocksdb
```
你将发现有几个G的磁盘占用。这是好事，说明写盘了。

现在我们需要知道多少键值被写盘。我们用redis-cli连入RedRock
```
./redis-cli
在redis-cli里执行rock report
```
你会在服务端RedRock窗口看到下面的信息
```
db=0, at lease one rock key = 794673
db=0, key total = 1000000, rock key = 994124, percentage = 99%, hot key = 5876, shared = 0, stream = 0
all db key total = 1000000, other total = 5876
```
第一行是打印出一个值在磁盘上的键。 

你可以用get命令获得它的值，它的值应该是类似下面的字样：'01234......9999'.

'db=0, key total', is current keys in mememory, it is one million. it is Great!

第二行说明，有99412键，其值在磁盘 (注意: 每台机器，每次运行，此值各不相同，但会是同数量级), 我们同事发现99%的键的值在磁盘。

Hot key是那些值还在内存的键。这些键未来很有可能继续被写入到磁盘。

Shared key是类似Java, Python那样，将一些常用的值做共享。详细可参考：Java Interning String，或者Python的常用数字-128~127。你大概了解，不用太关心，因为这样的键不会太多。

Steam键是不会写入磁盘的。所以，它也不会是HotKey。

Other total不用太关心，一般而言，就是HotKey。

### 第二测试用例：K/V: 四百万 maxmemory-only-for-rocksdb == yes

如果我们像上面那样，但是四百万个键值，会如何 
```
_warm_up_with_string(4_000_000)
```
很显然，100 MB memory对于四百个键和值，光键都装不下。

运行一段时间，你会在python里看到下面的错误和异常
```
redis.exceptions.ResponseError: OOM command not allowed when used memory > 'maxmemory'.
```

这是对的!!! Redis也是这么做。因为内存根本无法容纳这么多的键。

RedRock只能拒绝服务，抛出OOM异常.

我们再用
```
rock report
```
看看结果 
```
db=0, key total = 2058037, rock key = 2058037, percentage = 100%, hot key = 0, shared = 0, stream = 0
```

你会发现，有2058037个键被写到RedRock的100兆内存里了(注意：各机器各时间此值不一定相同，但数量级差不多) ,
而且这些键的所有的值都在磁盘上，因此报告里是100%.

### 第三测试用例：K/V: 一百万 maxmemory-only-for-rocksdb == no

```
./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --save ""
```
我们再运行 
```
rock report
```
报告和第一用例测试差不多。

这是正确的，想想为什么？

### 第四测试用例：K/V: 四百万 maxmemory-only-for-rocksdb == no 同时启动 eviction

```
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb no --save "" --maxmemory-policy allkeys-random --bind 0.0.0.0
```

然后python里运行
```
_warm_up_with_string(4_000_000)
```
和第三个案例不一样，没有任何报错和异常！

我们再运行'rock report'

报告如下： 
```
db=0, key total = 1907718, rock key = 1906765, percentage = 99%, hot key = 953, shared = 0, stream = 0
```
我们尝试加入四百万个键和值，只有近一半在内存里。其他其实都加成功了，只是被后来的弹出去了（eviction）。因此，不会有OOM异常。

### 第五测试用例：for K/V: 不光插入百万，同时还验证数据值

```
sudo ./redis-server --maxmemory 100m --enable-rocksdb-feature yes --maxmemory-only-for-rocksdb yes --save "" --bind 0.0.0.0
Python run 
_warm_up_with_string(1_000_000)
_check_all_key_in_string()
```
等着结果（不会有错误信息的，你会成功的！）

在testredrock目录下查看test_redrock.py了解更多.




