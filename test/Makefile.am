
if WITH_CUTTER

TESTS = run-test.sh
TESTS_ENVIRONMENT = NO_MAKE=yes CUTTER="$(CUTTER)"

noinst_LTLIBRARIES = test-micbench-io.la

LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined
INCLUDES = $(CUTTER_CFLAGS) -I$(top_srcdir)/src
LIBS = $(CUTTER_LIBS) \
	$(top_builddir)/src/libmicbench-io.la

test_micbench_io_la_SOURCES = test-micbench-io.c

echo-cutter:
	@echo $(CUTTER)

endif