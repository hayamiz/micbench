#ifndef MICBENCH_TEST_MOCKS_H
#define MICBENCH_TEST_MOCKS_H

#define _GNU_SOURCE

#include <glib.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <execinfo.h>

#include "micbench-test-utils.h"

typedef struct {
    const char *fname;
    const void **fargs;
    const char **backtrace;
    int num_backtrace;
} mb_mock_hentry_t; // entry of history

/* ---- utility functions ---- */
void   mb_mock_history_set(GList *history);
GList *mb_mock_history_get(void);
void   mb_mock_history_append(mb_mock_hentry_t *entry);
void   mb_mock_history_destroy(void);

mb_mock_hentry_t *mb_mock_hentry_make(const char *fname, const void **fargs);
void              mb_mock_hentry_destroy(mb_mock_hentry_t *entry);

const void *mb_mock_arg_int(int val);


#define BACKTRACE_MAX 10
#define mb_mock_collect_backtrace(entry)                                \
    {                                                                   \
        void *__bt_array[BACKTRACE_MAX];                                \
        (entry)->num_backtrace = backtrace(__bt_array, BACKTRACE_MAX);  \
        (entry)->backtrace = backtrace_symbols(__bt_array,              \
                                               (entry)->num_backtrace); \
        cut_take_memory((entry)->backtrace);                            \
    }

#define MOCK_HISTORY_RECORD(fname, ...)                                 \
    {                                                                   \
        mb_mock_hentry_t *__entry;                                      \
        __entry = mb_mock_hentry_make(fname,                            \
                                      mb_take_gpointer_array(__VA_ARGS__, \
                                                             NULL));    \
        mb_mock_collect_backtrace(__entry);                             \
        mb_mock_history_append(__entry);                                \
    }

#endif
