[回中文总目录](menu_cn.md)

# 编译

## 预备环境

本工程基于C/C++，你需要有gcc/g++，并能支持make

## 下载和编译

从GitHub下载本工程后
```
git clone https://github.com/szstonelee/redrock redrock
cd redrock
cd src
make
```
如果成功，会在编译目录redrock/src下看到redis-server这个文件
```
ls -all redis-server
```
如果看到，恭喜你，你已经编译成功，并获得了RedRock。
然后你可以执行看看，
```
./redis-server
```
用redis-cli连如试试，简单测试一下set, get

## 编译选项
### 底层重新编译
如果你需要从头编译，包括编译各种支持库，如Rocksdb, Snappy, Jemalloc
```
cd src
make distclean
make
```
温馨提示：上面这个编译很耗时间，主要是Rocksdb的编译很漫长，你可以喝完一杯咖啡再回来看结果
### 源代码重新编译
不需要重新基础库，但对于src目录下的所有源文件编译
```
cd src
make clean
make
```
### 快速编译
如果你只改了src下某个文件，最快的编译如下
```
make
```


