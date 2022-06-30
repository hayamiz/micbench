/* -*- indent-tabs-mode: nil -*- */

#ifndef MICBENCH_IO_H
#define MICBENCH_IO_H

#include "micbench.h"

#include <libaio.h>

typedef enum {
    PATTERN_SEQ,
    PATTERN_RAND,
    PATTERN_SEEKDIST,
    PATTERN_SEEKINCR,
} mb_io_pattern_t;

typedef enum {
    AIO_LIBAIO,

#ifdef HAVE_IO_URING
    AIO_IOURING,
#endif
} mb_aio_engine_t;

typedef struct {
    // multiplicity of IO
    int multi;

    // access mode
    mb_io_pattern_t pattern;
    bool direct;
    bool read;
    bool write;
    double rwmix;
    bool aio;

    mb_aio_engine_t aio_engine;

    // aio nr_events per threads
    int aio_nr_events;

    // file name of trace log of aio events
    char *aio_tracefile;

    // I/O activity log file
    char *logfile_path;
    FILE *logfile;

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

    // stride size of seekdist (should be much greater than on-disk read buffer)
    int64_t seekdist_stride;

    int64_t misalign;

    // device or file
    int open_flags;
    int nr_files;
    char **file_path_list;
    int64_t *file_size_list;

    // bogus computation
    long bogus_comp; // # of computation to be operated

    useconds_t iosleep;

    bool continue_on_error;

    bool json;

    bool verbose;

    bool noop;
} micbench_io_option_t;

typedef enum {
    MB_DO_READ,
    MB_DO_WRITE,
} mb_io_mode_t;

typedef struct mb_res_pool_cell {
    void *data;
    struct mb_res_pool_cell *next;
} mb_res_pool_cell_t;

// resource pool
typedef struct {
    int nr_elems;
    int nr_avail;
    mb_res_pool_cell_t *ring; // ring buffer
    mb_res_pool_cell_t *head;
    mb_res_pool_cell_t *tail;

    size_t elemsize;
    char *elembase;
} mb_res_pool_t;


/* wrapper of struct iocb */
typedef struct aiom_cb {
    struct iocb iocb;
    struct timeval submit_time;
    struct timeval queue_time;
    int file_idx;
    struct iovec *vec;
    int iovec_idx;
} aiom_cb_t;

// AIO manager
typedef struct {
    io_context_t context;

#ifdef HAVE_IO_URING
    struct io_uring uring;
#endif

    mb_res_pool_t *cbpool;
    struct iovec *vecs;

    int nr_events;
    int nr_pending;
    int nr_inflight;

    // # of IO completed by this AIO manager
    int64_t iocount;
    double iowait;

    aiom_cb_t **pending;
    struct io_event *events;
} mb_aiom_t;


void mb_set_option(micbench_io_option_t *option);
int parse_args(int argc, char **argv, micbench_io_option_t *option);

mb_aiom_t   *mb_aiom_make           (int nr_events);
void         mb_aiom_destroy        (mb_aiom_t *aiom);
void         mb_aiom_submit         (mb_aiom_t *aiom);
aiom_cb_t   *mb_aiom_prep_pread     (mb_aiom_t *aiom, int fd, int file_idx,
                                     aiom_cb_t *aiom_cb, size_t count, long long offset);
aiom_cb_t   *mb_aiom_prep_pwrite    (mb_aiom_t *aiom, int fd, int file_idx,
                                     aiom_cb_t *aiom_cb, size_t count, long long offset);
int          mb_aiom_wait           (mb_aiom_t *aiom, struct timespec *timeout);
int          mb_aiom_waitall        (mb_aiom_t *aiom);
int          mb_aiom_nr_submittable (mb_aiom_t *aiom);

mb_res_pool_t *mb_res_pool_make    (int nr_elems);
void           mb_res_pool_destroy (mb_res_pool_t *pool);
void          *mb_res_pool_pop     (mb_res_pool_t *pool);
int            mb_res_pool_push    (mb_res_pool_t *pool, void *elem);
int64_t        mb_res_pool_idx     (mb_res_pool_t *pool, void *elem);

#define mb_read_or_write() \
    (option.read == true ? MB_DO_READ : \
     option.write == true ? MB_DO_WRITE : \
     (option.rwmix < drand48() ? MB_DO_READ :   \
      MB_DO_WRITE))


int micbench_io_main(int argc, char **argv);

#endif
