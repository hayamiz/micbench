
if WITH_CUTTER
if WITH_GLIB

noinst_LTLIBRARIES =				\
	libmicbench-test-utils.la		\
	libmicbench-test-mocks.la

libmicbench_test_utils_la_SOURCES = micbench-test-utils.c

libmicbench_test_mocks_la_SOURCES = micbench-test-mocks.c
libmicbench_test_mocks_la_LIBADD = libmicbench-test-utils.la

LDFLAGS += -module -rpath $(libdir) -avoid-version -no-undefined	\
	$(GLIB_LDFLAGS)
INCLUDES = $(GLIB_CFLAGS) $(CUTTER_CFLAGS)
LIBS += $(CUTTER_LIBS)

endif
endif