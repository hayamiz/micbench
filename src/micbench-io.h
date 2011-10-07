#ifndef MICBENCH_IO_H
#define MICBENCH_IO_H

#include "micbench.h"

#include <libaio.h>

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
    bool aio;

    // aio nr_events per threads
    int aio_nr_events;

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

typedef struct mb_iocb_pool_cell {
    struct iocb *iocb;
    struct mb_iocb_pool_cell *next;
} mb_iocb_pool_cell_t;

// AIO control block pool
typedef struct {
    int size;
    int nfree;
    mb_iocb_pool_cell_t *iocbs; // ring buffer
    mb_iocb_pool_cell_t *head;
    mb_iocb_pool_cell_t *tail;
} mb_iocb_pool_t;


// AIO manager
typedef struct {
    int nr_events;
    io_context_t context;
    mb_iocb_pool_t *cbpool;
} mb_aiom_t;


void mb_set_option(micbench_io_option_t *option);
int parse_args(int argc, char **argv, micbench_io_option_t *option);

mb_aiom_t *mb_aiom_make         (int nr_events);
void       mb_aiom_destroy      (mb_aiom_t *aiom);
int        mb_aiom_submit       (mb_aiom_t *aiom, int nr, struct iocb **iocbpp);
int        mb_aiom_submit_pread (mb_aiom_t *aiom, int fd, void *buf, size_t count, long long offset);
int        mb_aiom_submit_pwrite(mb_aiom_t *aiom, int fd, void *buf, size_t count, long long offset);

mb_iocb_pool_t *mb_iocb_pool_make    (int nr_events);
void            mb_iocb_pool_destroy (mb_iocb_pool_t *pool);
struct iocb *   mb_iocb_pool_pop     (mb_iocb_pool_t *pool);
int             mb_iocb_pool_push    (mb_iocb_pool_t *pool, struct iocb *iocb);

#define mb_read_or_write() \
    (option.read == true ? MB_DO_READ : \
     option.write == true ? MB_DO_WRITE : \
     (option.rwmix < drand48() ? MB_DO_READ :   \
      MB_DO_WRITE))


int micbench_io_main(int argc, char **argv);

#endif
