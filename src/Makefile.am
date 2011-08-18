
AM_CFLAGS = -g -Wall
LDFLAGS =

bin_PROGRAMS = micbench-io micbench-mem micbench-lock micbench-proto

micbench_io_SOURCES = micbench-io.c micbench-utils.c

micbench_io_LDFLAGS = \
	-pthread

micbench_mem_SOURCES =  micbench-mem.c micbench-utils.c

micbench_mem_LDFLAGS = \
	-pthread

micbench_lock_SOURCES =  micbench-lock.c micbench-utils.c

micbench_lock_LDFLAGS = \
	-pthread

micbench_proto_SOURCES =  micbench-proto.c micbench-utils.c

micbench_proto_LDFLAGS = \
	-pthread

CLEANFILES = micbench-mem-inner.c micbench-mem-inner-64.c micbench-mem-inner-rand.c __micbench-io __micbench-mem

__micbench-%: micbench-%
	strip -o $@ $<

all: __micbench-io __micbench-mem

micbench-mem.c: micbench-mem-inner.c micbench-mem-inner-rand.c micbench-mem-inner-64.c

micbench-lock.c: micbench-lock-mfence-inner.c

micbench-mem-inner.c: ./gen-mem-innerloop.rb
	ruby $^ > $@
micbench-mem-inner-64.c: ./gen-mem-innerloop.rb
	ruby $^ --short 64 > $@

micbench-mem-inner-rand.c: ./gen-mem-innerloop.rb
	ruby $^ --random > $@

micbench-lock-mfence-inner.c: Makefile
	for i in $$(seq 128); do echo "\"incq	(%%rax);\\n\""; echo "\"mfence\\n\""; done > $@
