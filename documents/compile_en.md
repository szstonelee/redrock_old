[Back Top Menu](../README.md)

# Compile

## Enviroment for Compilation

RedRock and all its dependencies are based on C/C++, so you need 
1. gcc/g++
2. make/cmake

In MAC, use brew. In Linux, use apt or apt-get to intall these softwares.

Now, in Linux, We use Jemalloc.

## First Setup

Clone RedRock from Github, download submodule Rocksdb, then make.
```
git clone https://github.com/szstonelee/redrock redrock
cd redrock
git submodule init
git submodule update
cd src
make all
```
If no error shows and a LOGN time of compilation with a lot of warnings, 

you will see an execute file named as redis-server in redrock/src folder.

NOTE: do not worry about the warnings, the makefile is for comparatible with old C(C99).
```
ls -all redis-server
```
Congratulation! You compiled sucessfully. 
You can go on to try it.
```
./redis-server
```
Use client tools like redis-cli, and test the basic commands like set/get.

## Compile Options
### From the bottom including every library
If you want everything new from beginning, including compiling each base library like Rocksdb, Snappy
```
cd src
make distclean
make
```
Tip:  
When compiling the above way like borning baby, it takes LONG time.  
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


