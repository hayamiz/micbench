
AM_CFLAGS = $(GLIB_CFLAGS)
LDFLAGS = -Wall

bin_PROGRAMS = micbench-io micbench-mem

micbench_io_SOURCES = micbench-io.c

micbench_io_LDADD =				\
	$(GLIB_LIBS)

micbench_mem_SOURCES =  micbench-mem.c micbench-utils.c

micbench_mem_LDADD =				\
	-lnuma					\
	-lm					\
	$(GLIB_LIBS)

CLEANFILES = micbench-mem-inner.c micbench-mem-inner-rand.c

micbench-mem.c: micbench-mem-inner.c micbench-mem-inner-rand.c

micbench-mem-inner.c: ./gen-mem-innerloop.rb
	ruby $^ > $@

micbench-mem-inner-rand.c: ./gen-mem-innerloop.rb
	ruby $^ --random > $@
