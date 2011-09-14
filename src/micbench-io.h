#ifndef MICBENCH_IO_H
#define MICBENCH_IO_H

#include "micbench.h"

typedef struct {
    // multiplicity of IO
    int multi;

    // access mode
    bool seq;
    bool rand;
    bool direct;
    bool read;
    bool write;

    // thread affinity assignment
    mb_affinity_t **affinities;

    // timeout
    int timeout;

    // block size
    char *blk_sz_str;
    int blk_sz;

    // offset
    int64_t ofst_start;
    int64_t ofst_end;

    int64_t misalign;

    // device or file
    const char *path;

    // bogus computation
    long bogus_comp; // # of computation to be operated

    bool verbose;

    bool noop;
} micbench_io_option_t;

void parse_args(int argc, char **argv, micbench_io_option_t *option);
int micbench_io_main(int argc, char **argv);

#endif
