# Redis dependency Makefile

uname_S:= $(shell sh -c 'uname -s 2>/dev/null || echo not')

CCCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
MAKECOLOR="\033[32;1m"
ENDCOLOR="\033[0m"

default:
	@echo "Explicit target required"

.PHONY: default

# Prerequisites target
.make-prerequisites:
	@touch $@

# Clean everything when CFLAGS is different
ifneq ($(shell sh -c '[ -f .make-cflags ] && cat .make-cflags || echo none'), $(CFLAGS))
.make-cflags: distclean
	-(echo "$(CFLAGS)" > .make-cflags)
.make-prerequisites: .make-cflags
endif

# Clean everything when LDFLAGS is different
ifneq ($(shell sh -c '[ -f .make-ldflags ] && cat .make-ldflags || echo none'), $(LDFLAGS))
.make-ldflags: distclean
	-(echo "$(LDFLAGS)" > .make-ldflags)
.make-prerequisites: .make-ldflags
endif

distclean:
	-(cd hiredis && $(MAKE) clean) > /dev/null || true
	-(cd linenoise && $(MAKE) clean) > /dev/null || true
	-(cd lua && $(MAKE) clean) > /dev/null || true
	-(cd jemalloc && ./autogen.sh && $(MAKE) distclean) > /dev/null || true
	-(cd rocksdb && $(MAKE) clean && rm -f librocksdb.a) > /dev/null || true
	-(cd zlib-1.2.11 && $(MAKE) distclean) > /dev/null || true
	-(cd snappy && rm -rf build) > /dev/null || true
	-(cd lz4 && $(MAKE) clean && rm -f lib/liblz4.a) > /dev/null || true
	-(cd staticlib && rm -f libz.a && rm -f libsnappy.a && rm -f librocksdb.a) > /dev/null || true
	-(cd bzip2 && $(MAKE) clean && rm -f libbz2.a) > /dev/null || true
	-(cd zstd && $(MAKE) clean && rm -f lib/libzstd.a) > /dev/null || true
	-(rm -f .make-*)

.PHONY: distclean

ifeq ($(BUILD_TLS),yes)
    HIREDIS_MAKE_FLAGS = USE_SSL=1
endif

rocksdb:
	cd rocksdb &&  make static_lib && cp librocksdb.a ../staticlib/

.PHONY: rocksdb

zlib: .make-prerequisites
	cd zlib-1.2.11 && ./configure --static && $(MAKE) && cp libz.a ../staticlib/

.PHONY: zlib 

snappy: 
	cd snappy && mkdir -p build && cd build && cmake ../ && make && cp libsnappy.a ../../staticlib/

.PHONY: snappy	

lz4:
	cd lz4 && make && cp lib/liblz4.a ../staticlib/

.PHONY: lz4

bzip2:
	cd bzip2 && make && cp libbz2.a ../staticlib/

.PHONY: bzip2

zstd:
	cd zstd && make && cp lib/libzstd.a ../staticlib/

.PHONY: zstd

hiredis: .make-prerequisites
	@printf '%b %b\n' $(MAKECOLOR)MAKE$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR)
	cd hiredis && $(MAKE) static $(HIREDIS_MAKE_FLAGS)

.PHONY: hiredis

linenoise: .make-prerequisites
	@printf '%b %b\n' $(MAKECOLOR)MAKE$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR)
	cd linenoise && $(MAKE)

.PHONY: linenoise

ifeq ($(uname_S),SunOS)
	# Make isinf() available
	LUA_CFLAGS= -D__C99FEATURES__=1
endif

LUA_CFLAGS+= -O2 -Wall -DLUA_ANSI -DENABLE_CJSON_GLOBAL -DREDIS_STATIC='' $(CFLAGS)
LUA_LDFLAGS+= $(LDFLAGS)
# lua's Makefile defines AR="ar rcu", which is unusual, and makes it more
# challenging to cross-compile lua (and redis).  These defines make it easier
# to fit redis into cross-compilation environments, which typically set AR.
AR=ar
ARFLAGS=rcu

lua: .make-prerequisites
	@printf '%b %b\n' $(MAKECOLOR)MAKE$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR)
	cd lua/src && $(MAKE) all CFLAGS="$(LUA_CFLAGS)" MYLDFLAGS="$(LUA_LDFLAGS)" AR="$(AR) $(ARFLAGS)"

.PHONY: lua

JEMALLOC_CFLAGS= -std=gnu99 -Wall -pipe -g3 -O3 -funroll-loops $(CFLAGS)
JEMALLOC_LDFLAGS= $(LDFLAGS)

jemalloc: .make-prerequisites
	@printf '%b %b\n' $(MAKECOLOR)MAKE$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR)
	cd jemalloc && ./autogen
	cd jemalloc && ./configure --with-version=5.1.0-0-g0 --with-lg-quantum=3 --with-jemalloc-prefix=je_ --enable-cc-silence CFLAGS="$(JEMALLOC_CFLAGS)" LDFLAGS="$(JEMALLOC_LDFLAGS)"
	cd jemalloc && $(MAKE) CFLAGS="$(JEMALLOC_CFLAGS)" LDFLAGS="$(JEMALLOC_LDFLAGS)" lib/libjemalloc.a

.PHONY: jemalloc

all:
	make jemalloc
	make rocksdb
	make zlib
	make snappy
	make lz4
	make bzip2
	make zstd
	make hiredis
	make linenoise
	make lua

.PHONY: all
