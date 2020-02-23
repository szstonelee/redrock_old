[Back Top Menu](../README.md)

# Compile

## Enviroment for Compile

RedRock and all its dependencies are based on C/C++, so you need 
1. gcc/g++
2. make.

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
Congratulation! You compiled sucessfully. 
You can go on to try it.
```
./redis-server
```
Use client tools like redis-cli, and test the basic commands like set/get.

## Compile Options
### From Base including every library
If you want everything new from beginning, including compiling each base library like Rocksdb, Snapy, Jemalloc
```
cd src
make distclean
make
```
Tip:  
When compiling the above way like borning baby, it takes LONG time.  
You can take a rest to have a cup of coffee.
### Only compile all source codes in src folder
we do not compile from the base, because the deps library is stable.
We only want a fresh compile. You can compile only all the source codes in the src folder.
```
cd src
make clean
make
```
### Quickest Compile
Most of times, when you changed just one source file, the quickest way to compile is: 
```
make
```


