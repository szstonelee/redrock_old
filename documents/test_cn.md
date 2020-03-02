[回中文总目录](menu_cn.md)

# 测试

## Redis的元测试

[编译完成后](compile_cn.md)，首先进行Redis自带的测试工具，它是用tcl编写的，所以，需要安装tcl，然后
```
cd src
make test
```
注意，在Linux环境下，测试必须设置一个环境，否则最后一个测试52号会失败
具体提示可以启动redis-server，会给下面一个提示
```
WARNING you have Transparent Huge Pages (THP) support enabled in your kernel. This will create latency and memory usage issues with Redis. To fix this issue run the command 'echo never > /sys/kernel/mm/transparent_hugepage/enabled' as root, and add it to your /etc/rc.local in order to retain the setting after a reboot. Redis must be restarted after THP is disabled.
```
你需要看到
```
cat /sys/kernel/mm/transparent_hugepage/enabled
```
看到其值为never时，测试才可能全部通过

有两个方法：
1. 临时方法
```
sudo su
echo never > /sys/kernel/mm/transparent_hugepage/enabled
```
2. 永久解决
编辑 /etc/default/grub，在文件里加入
```
GRUB_CMDLINE_LINUX_DEFAULT="transparent_hugepage=never quiet splash"
```
然后，运行 update-grub。最后，重启整个Linux

## 
