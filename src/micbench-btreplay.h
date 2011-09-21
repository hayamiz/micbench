#ifndef MICBENCH_BTREPLAY_H
#define MICBENCH_BTREPLAY_H

#include "micbench.h"
#include "blktrace_api.h"

#include <glib.h>

typedef struct {
    bool verbose;
    bool vverbose;
    bool direct;

    int multi;

    int timeout;
    bool repeat;

    const char *btdump_path;
    const char *target_path;
} mb_btreplay_option_t;

typedef struct {
    int tid;
    GAsyncQueue *ioreq_queue;
} mb_btreplay_thread_arg_t;

typedef struct {
    bool stop;
    struct blk_io_trace trace;
} mb_btreplay_ioreq_t;

int mb_btreplay_parse_args(int argc, char **argv, mb_btreplay_option_t *option);
int mb_fetch_blk_io_trace(FILE *file, struct blk_io_trace *trace);

int mb_btreplay_main(int argc, char **argv);

#endif
