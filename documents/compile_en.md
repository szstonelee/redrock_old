[Back Top Menu](../README.md)

# Compile

## Enviroment for Compile

Rocksdb base on C/C++, so you need gcc/g++, and make.

## Setup

Clone RedRock from github, then make.
```
git clone https://github.com/szstonelee/redrock redrock
cd redrock
cd src
make
```
If no error shows, you will see an execute file named as redis-server in redrock/src folder.
```
ls -all redis-server
```
Congratulation, you compile and make sucessfully. 
You can go on to try it.
```
./redis-server
```
Use redis-cli such client tools, and test the basic commands like set/get.

## Compile Options
### From Base, every library
If you want everything new from beginning, including compile each base library like Rocksdb, Snapy, Jemalloc
```
cd src
make distclean
make
```
Tip: When compiling from the base, it takes a lot of time. You can take a rest to have a cup of coffee.
### Only Compile All Source Codes
we do not compile from the base, because the library is stable.
We only want a fresh compile. You can compile only all the source codes.
```
cd src
make clean
make
```
### Quick Compile
Most of times, you changed one source file, and just compile that. 
```
make
```
This is the quickest way of compiling.


