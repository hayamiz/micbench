
AM_CFLAGS = -g -Wall -msse -msse2 -std=c99
LDFLAGS = -no-undefined
INCLUDES = 

noinst_LTLIBRARIES =				\
	libmicbench-io.la			\
	libmicbench-utils.la

bin_PROGRAMS = micbench-io micbench-mem micbench-proto
if WITH_X86_64
bin_PROGRAMS += micbench-lock
endif

bin_SCRIPTS = micbench

EXTRA_DIST =					\
	micbench				\
	gen-mem-innerloop.rb


micbench_headers =				\
	micbench.h				\
	micbench-utils.h			\
	micbench-io.h				\
	micbench-btreplay.h			\
	blktrace_api.h


libmicbench_utils_la_SOURCES = micbench-utils.c

micbench_io_SOURCES = micbench-io-main.c $(micbench_headers)
micbench_io_LDADD = libmicbench-io.la libmicbench-utils.la
micbench_io_LDFLAGS = \
	-pthread
libmicbench_io_la_SOURCES = micbench-io.c

micbench_mem_SOURCES =  micbench-mem.c $(micbench_headers)
micbench_mem_LDADD =  libmicbench-utils.la
micbench_mem_LDFLAGS = \
	-pthread

if WITH_X86_64
micbench_lock_SOURCES =  micbench-lock.c $(micbench_headers)
micbench_lock_LDADD =  libmicbench-utils.la
micbench_lock_LDFLAGS = \
	-pthread
micbench-lock.c: micbench-lock-mfence-inner.c
micbench-lock-mfence-inner.c: Makefile
	for i in $$(seq 128); do echo "\"incq	(%%rax);\\n\""; echo "\"mfence\\n\""; done > $@
endif

micbench_proto_SOURCES =  micbench-proto.c $(micbench_headers)
micbench_proto_LDADD =  libmicbench-utils.la
micbench_proto_LDFLAGS = \
	-pthread

if WITH_BLKTRACE
if WITH_GLIB
INCLUDES	+= $(GLIB_CFLAGS)

noinst_LTLIBRARIES += libmicbench-btreplay.la
bin_PROGRAMS += micbench-btreplay

micbench_btreplay_SOURCES = micbench-btreplay-main.c $(micbench_headers)
micbench_btreplay_LDADD = libmicbench-btreplay.la libmicbench-utils.la
libmicbench_btreplay_la_SOURCES = micbench-btreplay.c
libmicbench_btreplay_la_LDFLAGS = -pthread $(GLIB_LIBS)
endif
endif

CLEANFILES = micbench-mem-inner.c micbench-mem-inner-64.c micbench-mem-inner-rand.c

micbench-mem.c: micbench-mem-inner.c micbench-mem-inner-rand.c micbench-mem-inner-64.c

if WITH_X86_64
micbench-mem-inner.c: ./gen-mem-innerloop.rb
	ruby $^ > $@
micbench-mem-inner-64.c: ./gen-mem-innerloop.rb
	ruby $^ --short 64 > $@
micbench-mem-inner-rand.c: ./gen-mem-innerloop.rb
	ruby $^ --random > $@
endif
if WITH_X86
micbench-mem-inner.c: ./gen-mem-innerloop.rb
	ruby $^ --32 > $@
micbench-mem-inner-64.c: ./gen-mem-innerloop.rb
	ruby $^ --32 --short 64 > $@
micbench-mem-inner-rand.c: ./gen-mem-innerloop.rb
	ruby $^ --32 --random > $@
endif
