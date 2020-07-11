[Back Top Menu](../README.md)

# Compile

It is very complicated and usually frustrated for compilation of a C/C++ project in MAC or Linux.

You need to be patient and figour out what is the problem.

## Enviroment for Compilation

RedRock and all its dependencies are based on C/C++, so you need 
1. gcc, g++
2. make, cmake
3. autoconf

In MAC, use brew. In Linux, use apt or apt-get to intall these softwares.

Now, in Linux, We use Jemalloc. Jemalloc is included with the project as the author of Redis, antirez, changed some source codes of that. (But the original Jemalloc can work with RedRock if you like.) And I have changed the source code of zlib-1.2.11 in deps folder, for there is a name confliction with the whole project, i.e. zmalloc or zfree function.

### in Linux, how install compilation tools
```
sudo apt upgrade
sudo apt -y install make
sudo apt -y install cmake
sudo apt -y install gcc
sudo apt -y install g++
sudo apt -y install autoconf
sudo apt -y install pkg-config
```

### in MAC, how install compilation tools

use brew to install make, cmake, gcc(g++), autoconf 
and install XCode for MAC in AppStroe (it needs you to register for Apple)

if brew, no need to install g++, only gcc is OK.

## First Setup, Compilation and Make

Clone RedRock from Github, download submodule Rocksdb, then make.
```
cd
git clone https://github.com/szstonelee/redrock redrock
cd redrock
git submodule init
git submodule update
cd src
make all
```

NOTE: 
1. 'git submodule' is for Rocksdb. It will use Rocksdb as a submodule project in RedRock project.
2. if gib submodule success, you can see a sub folder named 'rocksdb' in 'deps' folder

If no error shows up and after a **LONG** time of compilation with a lot of warnings, 

you will see an execute file named as **redis-server** in redrock/src folder.

NOTE: do not worry about the warnings, the makefile is for comparatible with old C(C99).
```
ls -all redis-server
```
Congratulation! You compiled sucessfully. 
You can go on to try it.
```
sudo ./redis-server
```
Use client tools like redis-cli, and test the basic commands like set/get.
```
./redis-cli
```
then in redis-cli window
```
set key val123
get key
```
NOTE: right now, in 'src' folder, there is a 'redis-cli' execute file, which is compiled by make. You can use it.

You can try test the redis-server by all redis test user cases. [Click here for more details.](test_en.md)

## Compile Options
### From the bottom including every library
If you want everything new from the beginning like a new life is coming, including compiling each base library like Rocksdb, Snappy
```
cd src
make distclean
make
```
Tip:  
When compiling the above way like borning a baby, it takes a LONG time.  
You can make a rest to have a cup of coffee concurrently.
### Only compile all source codes in src folder
We do not need to compile from the bottom every time, because the dependency libraries are stable.

We only want a fresh compilation. You can compile only all the source codes in the src folder.
```
cd src
make clean
make
```
### Quickest Compile
Most of times, when you change just one source file, the quickest way to compile is: 
```
make
```

## Next Step

[How config to achieve the RedRock features](howrun_en.md) or [How test all user cases for all features](test_en.md)
