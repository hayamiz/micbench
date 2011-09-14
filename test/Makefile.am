
if WITH_CUTTER
if WITH_GLIB

SUBDIRS = lib fixtures

TESTS = run-test.sh
TESTS_ENVIRONMENT = NO_MAKE=yes CUTTER="$(CUTTER)"

noinst_LTLIBRARIES =				\
	test-micbench-utils.la			\
	test-micbench-io.la

LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined	\
	 $(GLIB_LDFLAGS)
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

echo-cutter:
	@echo $(CUTTER)

endif
endif