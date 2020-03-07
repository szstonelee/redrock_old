[回中文总目录](menu_cn.md)

# 各种测试

我们需要对各种特性进行测试，这样你才能对RedRock有信心。

## 首先是Redis自带的测试工具

### 如何进行纯Redis测试

我们先要看看RedRock能否通过Redis自带的测试工具。

Redis带的测试工具时tcl软件编写，你需要在自己的操作系统里安装tcl.

接着
```
cd src
make
make test
```
注意: 执行make test时, 必须没有redis server在运行, 即6379是空闲的。

总共有52个步骤测试，这需要点时间。

### Linux Jemalloc生效后，mem defrag测试通过不了

我不知道。Redis源码也不过了这个测试（参考：https://github.com/antirez/redis）

这不影响当前的功能和特性，所以不用太担心。我会继续关注这个问题。

### Linux下一个注意事项：Transparent Huge Pages(THP)

很多Linux都一个缺省特性是打开的：Transparent Huge Pages.

如果是这样，第52个个性过不去（纯Redis也过不起）

#### 检测你的Linux是否开启了THP 

两个方法:

1. 运行redis-server (也可以是RedRock)
```
./redis-server
```
运行窗口里, 如果你的Linux打开了THG，那么你可以看到这个警告

2. 检查一个文件
```
sudo cat /sys/kernel/mm/redhat_transparent_huge
```
如果你没有看到'never'生效, 也就意味你的Linux启动了THP.

#### 在Linux如何永久关闭THP

用Google搜索方法!

我的方法：

```
sudo vi /etc/default/grub
add or modify the sentence:
RUB_CMDLINE_LINUX_DEFAULT="transparent_hugepage=never quiet splash"
then quit and save
then reboot
then check sudo cat /sys/kernel/mm/redhat_transparent_huge
```

## 我写的一个测试工具

这是一个Python脚本 

在Python3下运行，同时需要redis-client for Python模块 (比如：https://github.com/andymccurdy/redis-py)
```
cd testredrock
ls test_redrock.py
```

## [测试String Key/Value](test_cn_kv.md)

## [测试各种数据结构, 包括：String/List/Set/Hash/Zset/Geo/HyperLogLog/Stream](test_cn_alltypes.md)

## [测试Pipeline](test_cn_pipeline.md)

## [测试Blocking](test_cn_block.md)

## [测试Transaction](test_cn_transaction.md)

## [测试Script(Lua)](test_cn_lua.md)

## [测试RDB、AOF备份](test_cn_backup.md)

## [测试Replication](test_cn_replication.md)

## [测试LFU](test_cn_lfu.md)



