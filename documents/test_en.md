[Back Top Menu](../README.md)

# Test Cases

We can test all user cases for RedRock. So you can have the confidence that RedRock runs as you want.

## Pure Redis Tests

### How run tests for Pure Redis Tests

First and most, we need to know, whether RedRock can pass all Redis tests.

Redis include a test tool written by tcl. You need install tcl in your OS.
```
sudo apt install tcl
```
Then
```
cd src
make test
```
NOTE: when run make test, there should no redis-server is running, i.e. No 6379 port is being listened.

The test has  52 test user cases. It takes a long time for all tests, and you can have a cup of coffee.

### Sometimes Linux mem defrag can not pass when Jemalloc

I do not know why. The original Redis source code https://github.com/antirez/redis, does not pass right now.

But in MAC or Linux compilation without Jemalloc, all test cases pass.

And sometimes Linux with Jemalloc can pass, like try one more times.

I will give it an attention. But every feature and function is OK right now. Do not worry.

### Linux Special Note for Transparent Huge Pages(THP)

Some Linux has a default feature -- Transparent Huge Pages.

If this feature is on in Linux, the 52th test can not pass.

#### Check whether THP on in your Linux 

You can check whether it is on in your Linux by two ways:

1. run redis-server (it could be RedRock)
```
sudo ./redis-server
```
In the terminal window, if you see Transparent Huge Pages warning, it means your Linux enable the feature.
```
# WARNING you have Transparent Huge Pages (THP) support enabled in your kernel. This will create latency and memory usage issues with Redis. To fix this issue run the command 'echo never > /sys/kernel/mm/transparent_hugepage/enabled' as root, and add it to your /etc/rc.local in order to retain the setting after a reboot. Redis must be restarted after THP is disabled.
```

2. check the file
```
sudo cat /sys/kernel/mm/transparent_hugepage/enabled
```
or 
```
sudo cat /sys/kernel/mm/redhat_transparent_huge
```
If you see something other than 'never', it means your Linxu enable THP.

#### How disable THP in your Linux (permanently)

Google!

My way:

```
sudo vi /etc/default/grub
add or modify the sentence:
RUB_CMDLINE_LINUX_DEFAULT="transparent_hugepage=never quiet splash"
then quit and save
then sudo update-grub
then sudo reboot
then login and check again 
```

## My Test Case Tool

### Python3 Source Codes

Please check the python script for the tool, 

it needs Python3 & redis-client for Python (e.g. https://github.com/andymccurdy/redis-py)
```
cd testredrock
ls test_redrock.py
```
### How install Python3, Pip3, Redis Client For Python in Linux
```
sudo apt install python3
sudo apt install python3-pip
pip3 install redis
```
For MAC, use brew

### run python script with pipenv
由于Python的环境问题是个大挑战，比如常见的python2, python3，就是一个大麻烦。

在我的github下, 你可以用Pipenv来运行test_redrock.py.

```
pipenv run python3 test_redrock.py
```

## [Test String Key/Value](test_en_kv.md)

## [Test All Data Types, String/List/Set/Hash/Zset/Geo/HyperLogLog/Stream](test_en_alltypes.md)

## [Test Pipeline](test_en_pipeline.md)

## [Test Blocking](test_en_block.md)

## [Test Transaction](test_en_transaction.md)

## [Test Script(Lua)](test_en_lua.md)

## [Test RDB or AOF Backup](test_en_backup.md)

## [Test Replication](test_en_replication.md)

## [Test LFU (including LRU)](test_en_lfu.md)



