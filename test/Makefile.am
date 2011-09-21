
if WITH_CUTTER
if WITH_GLIB

SUBDIRS = lib fixtures

TESTS = run-test.sh
TESTS_ENVIRONMENT = NO_MAKE=yes CUTTER="$(CUTTER)"

noinst_LTLIBRARIES =				\
	test-micbench-utils.la			\
	test-micbench-io.la

LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined	\
	$(GLIB_LIBS)
INCLUDES =					\
	 $(CUTTER_CFLAGS)			\
	 $(GLIB_CFLAGS)				\
	-I$(top_srcdir)/test/lib		\
	-I$(top_srcdir)/src

LIBS = $(CUTTER_LIBS)					\
	$(top_builddir)/test/lib/libmicbench-test.la	\
	$(top_builddir)/src/libmicbench-utils.la	\
	$(top_builddir)/src/libmicbench-io.la

test_micbench_utils_la_SOURCES = test-micbench-utils.c
test_micbench_io_la_SOURCES = test-micbench-io.c

if WITH_BLKTRACE
noinst_LTLIBRARIES += test-micbench-btreplay.la

test_micbench_btreplay_la_SOURCES = test-micbench-btreplay.c

LIBS += $(top_builddir)/src/libmicbench-btreplay.la
endif

echo-cutter:
	@echo $(CUTTER)

endif
endif