
AM_CFLAGS = -g -O2 -Wall
LDFLAGS = -Wall

bin_PROGRAMS = micbench-io micbench-mem

micbench_io_SOURCES = micbench-io.c micbench-utils.c

micbench_io_LDFLAGS = \
	-pthread

micbench_mem_SOURCES =  micbench-mem.c micbench-utils.c

micbench_mem_LDFLAGS = \
	-pthread

CLEANFILES = micbench-mem-inner.c micbench-mem-inner-rand.c __micbench-io __micbench-mem

__micbench-%: micbench-%
	strip -o $@ $<

all: __micbench-io __micbench-mem

micbench-mem.c: micbench-mem-inner.c micbench-mem-inner-rand.c

micbench-mem-inner.c: ./gen-mem-innerloop.rb
	ruby $^ > $@

micbench-mem-inner-rand.c: ./gen-mem-innerloop.rb
	ruby $^ --random > $@
