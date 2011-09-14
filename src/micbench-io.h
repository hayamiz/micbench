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
    double rwmix;

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

typedef enum {
    MB_DO_READ,
    MB_DO_WRITE,
} mb_io_mode_t;

void mb_set_option(micbench_io_option_t *option);
int parse_args(int argc, char **argv, micbench_io_option_t *option);

#define mb_read_or_write() \
    (option.read == true ? MB_DO_READ : \
     option.write == true ? MB_DO_WRITE : \
     (option.rwmix < drand48() ? MB_DO_READ :   \
      MB_DO_WRITE))


int micbench_io_main(int argc, char **argv);

#endif
