#ifndef MICBENCH_TEST_UTILS_H
#define MICBENCH_TEST_UTILS_H

#include <glib.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cutter.h>
#include <gcutter.h>

const char *mb_test_get_test_dir (void);
const char *mb_test_get_fixtures_dir (void);

// must be NULL-terminated
const void **mb_take_gpointer_array(const void *ptr, ...);

#endif
