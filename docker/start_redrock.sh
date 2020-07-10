#!/bin/bash

echo never > /sys/kernel/mm/transparent_hugepage/enabled
echo 1024 > /proc/sys/net/core/somaxconn

/redrock/redis-server \
  --bind 0.0.0.0 \
  --maxmemory 100M \
  --enable-rocksdb-feature yes \
  --rockdbdir /redrock/rockdbdir/ \
  --maxmemory-only-for-rocksdb yes