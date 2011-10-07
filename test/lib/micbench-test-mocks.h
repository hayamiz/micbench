#ifndef MICBENCH_TEST_MOCKS_H
#define MICBENCH_TEST_MOCKS_H

#define _GNU_SOURCE

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <execinfo.h>

#include "micbench-test-utils.h"

typedef enum {
    MOCK_ARG_SKIP = 1,
    MOCK_ARG_INT,
    MOCK_ARG_PTR,
} mb_mock_arg_type_t;

typedef struct {
    mb_mock_arg_type_t type;
    union {
        int _int; // MOCK_ARG_INT
        void *_ptr; // MOCK_ARG_PTR
    } u;
} mb_mock_arg_t;

/* ---- utility functions ---- */

void mb_mock_init(void);
void mb_mock_finish(void);

/* mb_assert_will_call(const char *fname,
 *                     mb_mock_arg_type_t arg_type1, arg1,
 *                     ..., NULL)
 */
void mb_mock_assert_will_call(const char *fname, ...);

void mb_mock_check(const char *fname, ...);

#endif
