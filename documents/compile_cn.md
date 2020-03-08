[回中文总目录](menu_cn.md)

# 编译

注意：不管是Linux下，还是MAC下，编译一个像RedRock这样的C/C++工程非常复杂，有时令人气馁。

你需要足够的耐心，然后找出正确的道路。我这里会尽量列出各种帮助。

## 预备环境

本工程基于C/C++，你需要有gcc/g++，并能支持make/cmake，还有autoconf

在MAC下，参考brew，在Linux下，参考apt或apt-get

NOTE: Linux下，已经开始支持Jemalloc了。Jemalloc已经包含在本工程里，因为Redis的作者antirez修改了一些Jemalloc的源代码。不过，你可以用纯Jemalloc用于RedRock的编译，没有问题。同时，我个人也修改了deps目录下的zlib-1.2.11源代码，因为有个命名冲突，zmalloc or zfree.

### Linux下的准备

```
sudo apt install make
sudo apt install cmake
sudo apt install gcc
sudo apt install g++
sudo apt install autoconf
```

### Mac下的准备

用brew安装上面这几个软件。同时，不要忘了安装XCode(在AppStore里)，它需要你自己安装并向Apple注册。

## 下载和编译

从GitHub下载本工程后
```
git clone https://github.com/szstonelee/redrock redrock
cd redrock
git submodule init
git submodule update
cd src
make
```

注意：
1. 上面的'git submodule'是为了将Rocksdb这个工程作为本工程的子工程，所用到的git命令。
2. 如果git submodule运行成功，你在deps目录下，可以看到一个子目录，名字叫rocksdb

如果成功，会在编译目录redrock/src下看到redis-server这个文件。

是的，RedRock缺省执行文件就是redis一样

注意：为了和老的C兼容，因此会有大量的warning出现，但不用担心这个。
```
ls -all redis-server
```
如果看到，恭喜你，你已经编译成功，并获得了RedRock。
然后你可以执行看看，
```
./redis-server
```
用redis-cli连接试试，简单测试一下set, get
```
./redis-cli
```
在redis-cli窗口里， 执行redis的命令
```
set abc 123456
get abc
```
注意：当前src目录下，就有一个已经编译好的redis-cli执行文件，你可以使用之。

你可以执行所有的redis的测试用例. [点击这里了解细节](test_en.md)

## 编译选项
### 底层重新编译
如果你需要从头编译，包括编译各种支持库，如Rocksdb, Snappy, Jemalloc
```
cd src
make distclean
make
```
温馨提示：上面这个编译很耗时间，主要是Rocksdb library的编译很漫长，你可以喝完一杯咖啡再回来看结果
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

## 下一步

你需要了解：[如何配置，才能获得新特性](howrun_cn.md) or [如何测试所有的特性]](test_cn.md)



