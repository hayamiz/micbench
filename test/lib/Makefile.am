
if WITH_CUTTER
if WITH_GLIB

noinst_LTLIBRARIES =				\
	libmicbench-test.la

libmicbench_test_la_SOURCES = micbench-test-utils.c

LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined	\
	 $(GLIB_LDFLAGS)
INCLUDES = $(GLIB_CFLAGS)

endif
endif