/* -*- indent-tabs-mode: nil -*- */

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE

#include "micbench-io.h"

typedef struct {
    pid_t thread_id;
    cpu_set_t mask;
    unsigned long nodemask;
} thread_assignment_t;

static micbench_io_option_t option;
static FILE *aio_tracefile;
static __thread pid_t tid;

typedef struct {
    // accumulated iowait time
    double iowait_time;

    // io count (in blocks)
    int64_t count;
} meter_t;

typedef struct {
    long io_count;
    long io_bytes;

    double start_time;          /* in second (unix epoch time) */
    double exec_time;           /* in second */
    double iowait_time;         /* in second */
    int64_t count;
    double response_time;       /* in second */
    double iops;
    double bandwidth;           /* in bytes/sec */
} result_t;

typedef struct {
    int id;
    pthread_t *self;
    meter_t *meter;
    long common_seed;
    int tid;

    int *fd_list;
} th_arg_t;



static void mb_log_io_activity(struct timeval *issue_tv, struct timeval *complete_tv,
                               const char *file, int64_t blockaddr, int blocksz);


mb_aiom_t *
mb_aiom_make(int nr_events)
{
    mb_aiom_t *aiom;
    int i;
    aiom_cb_t *aiom_cb;
    int ret;

    aiom = malloc(sizeof(mb_aiom_t));

    aiom->nr_events = nr_events;
    aiom->nr_pending = 0;
    aiom->nr_inflight = 0;

    aiom->iocount = 0;

    aiom->pending = malloc(sizeof(aiom_cb_t *) * nr_events);
    if (aiom->pending == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    bzero(aiom->pending, sizeof(aiom_cb_t *) * nr_events);

    aiom->events = malloc(sizeof(struct io_event) * nr_events);
    if (aiom->events == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    bzero(aiom->events, sizeof(struct io_event) * nr_events);

    aiom->vecs = malloc(sizeof(struct iovec) * nr_events);
    if (aiom->vecs == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    bzero(aiom->vecs, sizeof(struct iovec) * nr_events);

    aiom->cbpool = mb_res_pool_make(nr_events);

    for(i = 0; i < nr_events; i++) {
        aiom_cb = malloc(sizeof(aiom_cb_t));
        if (aiom_cb == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        bzero(aiom_cb, sizeof(aiom_cb_t));
        if ((void *) aiom_cb != (void *) &aiom_cb->iocb)
        {
            fprintf(stderr, "Pointer mismatch: aiom_cb != &aiom_cb->iocb\n");
            exit(EXIT_FAILURE);
        }

        /* prepare buffer and iovec */
        aiom_cb->iovec_idx = i;
        aiom_cb->vec = &aiom->vecs[i];
        aiom_cb->vec->iov_len = option.blk_sz;
        aiom_cb->vec->iov_base = memalign(512, option.blk_sz);
        if (aiom_cb->vec->iov_base == NULL) {
            perror("malloc failed.");
            exit(EXIT_FAILURE);
        }
        bzero(aiom_cb->vec->iov_base, option.blk_sz);

        mb_res_pool_push(aiom->cbpool, aiom_cb);
    }

    bzero(&aiom->context, sizeof(io_context_t));
    #ifdef HAVE_IO_URING
    bzero(&aiom->uring, sizeof(struct io_uring));
    #endif

    switch(option.aio_engine) {
    case AIO_LIBAIO:
        if ((ret = io_setup(nr_events, &aiom->context)) != 0) {
            perror("io_setup failed.");
            return NULL;
        }
        break;
    #ifdef HAVE_IO_URING
    case AIO_IOURING:
        ret = io_uring_queue_init(nr_events, &aiom->uring, 0);
        if (ret != 0) {
            perror("io_uring_queue_init failed.");
            return NULL;
        }
        ret = io_uring_register_buffers(&aiom->uring, aiom->vecs, aiom->nr_events);
        if (ret != 0) {
            perror("io_uring_register_buffers failed.");
            return NULL;
        }
        break;
    #endif
    }

    return aiom;
}

void
mb_aiom_destroy (mb_aiom_t *aiom)
{
    aiom_cb_t *aiom_cb;

    free(aiom->pending);
    free(aiom->events);

    switch(option.aio_engine) {
    case AIO_LIBAIO:
        io_destroy(aiom->context);
        break;
#ifdef HAVE_IO_URING
    case AIO_IOURING:
        io_uring_queue_exit(&aiom->uring);
        break;
#endif
    }

    while((aiom_cb = mb_res_pool_pop(aiom->cbpool)) != NULL) {
        free(aiom_cb->vec->iov_base);
        free(aiom_cb);
    }
    mb_res_pool_destroy(aiom->cbpool);
    free(aiom->vecs);
    free(aiom);
}

void
mb_aiom_submit(mb_aiom_t *aiom)
{
    int i;
    int ret = 0;
    struct timeval submit_time;

    if (aiom->nr_pending == 0) {
        return;
    }

    GETTIMEOFDAY(&submit_time);
    for (i = 0; i < aiom->nr_pending; i++) {
        aiom_cb_t *cb;
        cb = aiom->pending[i];
        cb->submit_time = submit_time;
    }

    switch(option.aio_engine) {
    case AIO_LIBAIO:
        ret = io_submit(aiom->context, aiom->nr_pending,
                        (struct iocb **) aiom->pending);
        if (ret != aiom->nr_pending) {
            fprintf(stderr, "mb_aiom_submit:io_submit only %d of %d succeeded\n",
                    ret, aiom->nr_pending);
            exit(EXIT_FAILURE);
        } else if (ret < 0) {
            perror("mb_aiom_submit:io_submit failed");
        }
        break;
#ifdef HAVE_IO_URING
    case AIO_IOURING:
        ret = io_uring_submit(&aiom->uring);
        if (ret != aiom->nr_pending) {
            fprintf(stderr, "mb_aiom_submit:io_uring_submit only %d of %d succeeded\n",
                    ret, aiom->nr_pending);
            exit(EXIT_FAILURE);
        } else if (ret < 0) {
            perror("mb_aiom_submit:io_uring_submit failed");
            exit(EXIT_FAILURE);
        }
        break;
#endif
    }

    if (aio_tracefile != NULL) {
        fprintf(aio_tracefile,
                "[%d] submit: %d req\n",
                tid, ret);
    }

    aiom->nr_inflight += aiom->nr_pending;
    aiom->nr_pending = 0;
}

aiom_cb_t *
mb_aiom_prep_pread   (mb_aiom_t *aiom, int fd, int file_idx,
                      aiom_cb_t *aiom_cb, size_t count, long long offset)
{
#ifdef HAVE_IO_URING
    struct io_uring_sqe *sqe;
#endif

    switch(option.aio_engine) {
    case AIO_LIBAIO:
        io_prep_pread(&aiom_cb->iocb, fd,
                      aiom_cb->vec->iov_base, aiom_cb->vec->iov_len, offset);
        break;
#ifdef HAVE_IO_URING
    case AIO_IOURING:
        sqe = io_uring_get_sqe(&aiom->uring);
        io_uring_prep_read_fixed(sqe, fd, aiom_cb->vec->iov_base,
                                 aiom_cb->vec->iov_len,
                                 offset, aiom_cb->iovec_idx);
        io_uring_sqe_set_data(sqe, aiom_cb);
        break;
#endif
    }
    GETTIMEOFDAY(&aiom_cb->queue_time);
    aiom_cb->file_idx = file_idx;

    aiom->pending[aiom->nr_pending++] = aiom_cb;

    return aiom_cb;
}

aiom_cb_t *
mb_aiom_prep_pwrite  (mb_aiom_t *aiom, int fd, int file_idx,
                      aiom_cb_t *aiom_cb, size_t count, long long offset)
{
#ifdef HAVE_IO_URING
    struct io_uring_sqe *sqe;
#endif

    switch(option.aio_engine) {
    case AIO_LIBAIO:
        io_prep_pwrite(&aiom_cb->iocb, fd,
                       aiom_cb->vec->iov_base, aiom_cb->vec->iov_len, offset);
        break;
#ifdef HAVE_IO_URING
    case AIO_IOURING:
        sqe = io_uring_get_sqe(&aiom->uring);
        io_uring_prep_write_fixed(sqe, fd, aiom_cb->vec->iov_base,
                                  aiom_cb->vec->iov_len,
                                  offset, aiom_cb->iovec_idx);
        io_uring_sqe_set_data(sqe, aiom_cb);
        break;
#endif
    }
    GETTIMEOFDAY(&aiom_cb->queue_time);
    aiom_cb->file_idx = file_idx;

    aiom->pending[aiom->nr_pending++] = aiom_cb;

    return aiom_cb;
}

int
__mb_aiom_wait(mb_aiom_t *aiom, long min_nr, long nr,
               struct io_event *events, struct timespec *timeout){
    int i;
    int nr_completed = 0;
    aiom_cb_t *aiom_cb = NULL;
    struct io_event *event = NULL;

#ifdef HAVE_IO_URING
    int ret;
    struct io_uring_cqe *cqe = NULL;
#endif

    switch(option.aio_engine) {
    case AIO_LIBAIO:
        nr_completed = io_getevents(aiom->context, min_nr, nr, events, timeout);
        if (nr_completed < 0) {
            perror("__mb_aiom_wait:io_getevents failed");
            exit(EXIT_FAILURE);
        }
        aiom->nr_inflight -= nr_completed;
        aiom->iocount += nr_completed;
        break;
#ifdef HAVE_IO_URING
    case AIO_IOURING:
        nr_completed = min_nr;
        break;
#endif
    }

    if (aio_tracefile != NULL) {
        fprintf(aio_tracefile,
                "[%d] %d infl %d comp\n",
                tid, aiom->nr_inflight, nr_completed);
    }

    for(i = 0; i < nr_completed; i++){
        struct timeval t1;
        const char *file_path;

        switch(option.aio_engine) {
        case AIO_LIBAIO:
            event = &aiom->events[i];
            aiom_cb = (aiom_cb_t *) event->obj;
            if (!(event->res == option.blk_sz && event->res2 == 0)){
                fprintf(stderr, "__mb_aiom_wait: fatal error on completion: res = %ld, res2 = %ld\n",
                        (long) event->res, (long) event->res2);
            }
            break;
#ifdef HAVE_IO_URING
        case AIO_IOURING:
            ret = io_uring_wait_cqe(&aiom->uring, &cqe);
            if (ret != 0) {
                perror("__mb_aiom_wait:io_uring_wait_cqe_nr failed");
                exit(EXIT_FAILURE);
            }
            aiom_cb = (aiom_cb_t *) io_uring_cqe_get_data(cqe);
            io_uring_cqe_seen(&aiom->uring, cqe);
            break;
#endif
        }

        // TODO: callback or something
        GETTIMEOFDAY(&t1);

        aiom->iowait += mb_elapsed_time_from(&aiom_cb->submit_time);

        if (aiom_cb->file_idx == -1) {
            file_path = "(null)";
        } else {
            file_path = option.file_path_list[aiom_cb->file_idx];
        }
        mb_log_io_activity(&aiom_cb->submit_time, &t1,
                           file_path, aiom_cb->iocb.u.c.offset, option.blk_sz);

        mb_res_pool_push(aiom->cbpool, aiom_cb);
    }

    return nr_completed;
}

int
mb_aiom_wait(mb_aiom_t *aiom, struct timespec *timeout)
{
    return __mb_aiom_wait(aiom, 1, aiom->nr_inflight, aiom->events, timeout);
}

int
mb_aiom_waitall(mb_aiom_t *aiom)
{
    return __mb_aiom_wait(aiom, aiom->nr_inflight, aiom->nr_inflight,
                               aiom->events, NULL);
}

int
mb_aiom_nr_submittable(mb_aiom_t *aiom)
{
    return aiom->cbpool->nr_avail;
}

mb_res_pool_t *
mb_res_pool_make(int nr_elems)
{
    mb_res_pool_t *pool;
    mb_res_pool_cell_t *next_cell;
    mb_res_pool_cell_t *cell;
    int i;

    pool = malloc(sizeof(mb_res_pool_t));
    if (pool == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    bzero(pool, sizeof(mb_res_pool_t));

    pool->nr_elems = nr_elems;
    pool->nr_avail = 0;

    for(cell = NULL, i = 0; i < nr_elems; i++) {
        next_cell = cell;
        cell = malloc(sizeof(mb_res_pool_cell_t));
        if (cell == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        bzero(cell, sizeof(mb_res_pool_cell_t));
        cell->data = NULL;

        if (pool->tail == NULL)
            pool->tail = cell;
        cell->next = next_cell;
    }

    // make ring
    pool->ring = pool->head = cell;
    pool->tail->next = pool->head;

    return pool;
}

void
mb_res_pool_destroy(mb_res_pool_t *pool)
{
    mb_res_pool_cell_t *cell;
    mb_res_pool_cell_t *cell_next;

    for(cell_next = NULL, cell = pool->ring;
        cell_next != pool->ring; cell = cell_next){
        cell_next = cell->next;
        free(cell);
    }
    free(pool);
}

/* return NULL on empty */
void *
mb_res_pool_pop(mb_res_pool_t *pool)
{
    void *ret;
    if (pool->nr_avail == 0) {
        return NULL;
    }
    ret = pool->head->data;
    pool->head = pool->head->next;
    pool->nr_avail --;

    return ret;
}

/* return -1 on error, 0 on success */
int
mb_res_pool_push(mb_res_pool_t *pool, void *elem)
{
    if (pool->nr_avail == pool->nr_elems) {
        return -1;
    }
    pool->tail = pool->tail->next;
    pool->tail->data = elem;
    pool->nr_avail ++;

    return 0;
}


void
print_option()
{
    const char *pattern_str;

    switch(option.pattern) {
    case PATTERN_SEQ:
        pattern_str = "sequential";
        break;
    case PATTERN_RAND:
        pattern_str = "random";
        break;
    case PATTERN_SEEKDIST:
        pattern_str = "seekdist";
        break;
    case PATTERN_SEEKINCR:
        pattern_str = "seekincr";
        break;
    default:
        pattern_str = "(unknown)";
        break;
    }

    printf("{\n\
  \"params\": {\n\
    \"threads\": %d,\n\
    \"mode\": \"%s\",\n\
    \"pattern\": \"%s\",\n\
    \"blocksize_byte\": %d,\n\
    \"offset_start_blk\": %ld,\n\
    \"offset_end_blk\": %ld,\n\
    \"direct\": %s,\n\
    \"aio\": %s,\n\
    \"timeout_sec\": %d,\n\
    \"bogus_comp\": %ld,\n\
    \"iosleep\": %d\n\
  }\n\
}\n",
           option.multi,
           (option.read ? "read" :
            option.write ? "write" : "mix"),
           pattern_str,
           option.blk_sz,
           option.ofst_start,
           option.ofst_end,
           (option.direct ? "true" : "false"),
           (option.aio ? "true" : "false"),
           option.timeout,
           option.bogus_comp,
           option.iosleep
        );
}

void
print_result(result_t *result)
{
    printf("== result ==\n\
exec_time     %lf [sec]\n\
iops          %lf [blocks/sec]\n\
response_time %lf [sec]\n\
transfer_rate %lf [MiB/sec]\n\
accum_io_time %lf [sec]\n\
",
           result->exec_time,
           result->iops,
           result->response_time,
           result->bandwidth / MEBI,
           result->iowait_time);
}

void
print_result_json(result_t *result, bool only_params)
{
    const char *pattern_str;
    char *files_str;
    char *files_str_ptr;
    size_t files_str_len;

    switch(option.pattern) {
    case PATTERN_SEQ:
        pattern_str = "sequential";
        break;
    case PATTERN_RAND:
        pattern_str = "random";
        break;
    case PATTERN_SEEKDIST:
        pattern_str = "seekdist";
        break;
    case PATTERN_SEEKINCR:
        pattern_str = "seekincr";
        break;
    default:
        pattern_str = "(unknown)";
        break;
    }

    files_str_len = 4;
    if (NULL == (files_str = malloc(files_str_len))) {
        perror("malloc failed.");
        exit(EXIT_FAILURE);
    }
    files_str[0] = '[';
    files_str_ptr = files_str + 1;
    for (int i = 0; i < option.nr_files; i++) {
        int n;
        if (i == 0) {
            n = snprintf(files_str_ptr, files_str_len - 2 - (files_str_ptr - files_str),
                         "\"%s\"", option.file_path_list[i]);
        } else {
            n = snprintf(files_str_ptr, files_str_len - 2 - (files_str_ptr - files_str),
                         ", \"%s\"", option.file_path_list[i]);
        }
        if (n >= files_str_len - 2 - (files_str_ptr - files_str)) {
            size_t ofst = files_str_ptr - files_str;

            files_str_len *= 2;
            files_str = realloc(files_str, files_str_len);
            files_str_ptr = files_str + ofst;
            i--;
            continue;
        } else {
            files_str_ptr += n;
        }
    }
    *files_str_ptr = ']';
    *(files_str_ptr + 1) = '\0';

    printf("{\n\
  \"params\": {\n\
    \"threads\": %d,\n\
    \"mode\": \"%s\",\n\
    \"pattern\": \"%s\",\n\
    \"blocksize_byte\": %d,\n\
    \"offset_start_blk\": %ld,\n\
    \"offset_end_blk\": %ld,\n\
    \"direct\": %s,\n\
    \"aio\": %s,\n\
    \"aio_nr_events\": %d,\n\
    \"aio_engine\": \"%s\",\n\
    \"timeout_sec\": %d,\n\
    \"bogus_comp\": %ld,\n\
    \"iosleep\": %d,\n\
    \"files\": %s\n\
  }",
           option.multi,
           (option.read ? "read" :
            option.write ? "write" : "mix"),
           pattern_str,
           option.blk_sz,
           option.ofst_start,
           option.ofst_end,
           (option.direct ? "true" : "false"),
           (option.aio ? "true" : "false"),
           option.aio_nr_events,
           (option.aio_engine == AIO_LIBAIO ? "libaio" : "io_uring" ),
           option.timeout,
           option.bogus_comp,
           option.iosleep,
           files_str
        );

    if (only_params == true) {
        printf("\n}\n");
    } else {
        printf(",\n\
  \"counters\": {\n\
    \"io_count\": %ld,\n\
    \"io_bytes\": %ld\n\
  },\n\
  \"metrics\": {\n\
    \"start_time_unix\": %lf,\n\
    \"exec_time_sec\": %lf,\n\
    \"iops\": %lf,\n\
    \"transfer_rate_mbps\": %lf,\n\
    \"response_time_msec\": %lf,\n\
    \"accum_io_time_sec\": %lf\n\
  }\n\
}\n",
               result->io_count,
               result->io_bytes,
               result->start_time,
               result->exec_time,
               result->iops,
               result->bandwidth / MEBI,
               result->response_time * 1000.0,
               result->iowait_time
            );
    }
}

int
parse_args(int argc, char **argv, micbench_io_option_t *option)
{
    char optchar;
    int idx;

    // default values
    option->noop = false;
    option->multi = 1;
    option->affinities = NULL;
    option->timeout = 60;
    option->bogus_comp = 0;
    option->iosleep = 0;
    option->read = true;
    option->write = false;
    option->rwmix = 0.0;
    option->pattern = PATTERN_SEQ;
    option->direct = false;
    option->aio = false;
    option->aio_engine = AIO_LIBAIO;
    option->aio_nr_events = 64;
    option->aio_tracefile = NULL;
    option->blk_sz = 4 * KIBI;
    option->seekdist_stride = 16 * 1024;
    option->ofst_start = -1;
    option->ofst_end = -1;
    option->misalign = 0;
    option->continue_on_error = false;
    option->logfile = NULL;
    option->logfile_path = NULL;
    option->json = false;
    option->verbose = false;
    option->open_flags = O_RDONLY;

    option->file_path_list = NULL;
    option->file_size_list = NULL;

    optind = 1;
    while ((optchar = getopt(argc, argv, "+Nm:a:t:RSDIdAg:E:T:WM:b:s:e:B:z:c:i:Cl:jv")) != -1){
        switch(optchar){
        case 'N': // noop
            option->noop = true;
            break;
        case 'm': // multiplicity
            option->multi = strtol(optarg, NULL, 10);
            break;
        case 'a': // affinity
        {
            mb_affinity_t *aff;

            // check for -m option
            for(idx = optind; idx < argc; idx++){
                if (strcmp("-m", argv[idx]) == 0) {
                    perror("-m option must be specified before -a.\n");
                    goto error;
                }
            }
            if (option->affinities == NULL){
                option->affinities = malloc(sizeof(mb_affinity_t *) * option->multi);
                bzero(option->affinities, sizeof(mb_affinity_t *) * option->multi);
            }
            if ((aff = mb_parse_affinity(NULL, optarg)) == NULL){
                fprintf(stderr, "Invalid argument for -a: %s\n", optarg);
                goto error;
            }
            aff->optarg = strdup(optarg);
            option->affinities[aff->tid] = aff;
        }
            break;
        case 't': // timeout
            option->timeout = strtol(optarg, NULL, 10);
            break;
        case 'R': // random
            option->pattern = PATTERN_RAND;
            break;
        case 'S': // sequential
            option->pattern = PATTERN_SEQ;
            break;
        case 'D': // constant seek distance
            option->pattern = PATTERN_SEEKDIST;
            break;
        case 'I': // increasing seek distance
            option->pattern = PATTERN_SEEKINCR;
            break;
        case 'd': // direct IO
            option->direct = true;
            break;
        case 'A': // Asynchronous IO
            option->aio = true;
            break;
        case 'g': // AIO engine
            if (strcmp(optarg, "libaio") == 0) {
                option->aio_engine = AIO_LIBAIO;
            } else if (strcmp(optarg, "io_uring") == 0) {
#ifdef HAVE_IO_URING
                option->aio_engine = AIO_IOURING;
#else
                fprintf(stderr, "[ERROR] io_uring is not avaiable on this platform\n");
                goto error;
#endif
            } else {
                fprintf(stderr, "[ERROR] no such AIO engine: %s\n", optarg);
                goto error;
            }
            break;
        case 'E': // AIO nr_events for each thread
            option->aio_nr_events = strtol(optarg, NULL, 10);
            break;
        case 'T': // AIO trace log file
            option->aio_tracefile = strdup(optarg);
            break;
        case 'W': // write
            option->write = true;
            option->read = false;
            break;
        case 'M': // read/write mixture (0 = 100% read, 1.0 = 100% write)
            option->rwmix = strtod(optarg, NULL);
            option->read = false;
            option->write = false;
            if (option->rwmix < 0)   option->rwmix = 0.0;
            if (option->rwmix > 1.0) option->rwmix = 1.0;
            break;
        case 'b': // block size
            option->blk_sz = strtol(optarg, NULL, 10);
            break;
        case 's': // start block
            option->ofst_start = strtol(optarg, NULL, 10);
            break;
        case 'e': // end block
            option->ofst_end = strtol(optarg, NULL, 10);
            break;
        case 'B': // stride size of seekdist
            option->seekdist_stride = strtol(optarg, NULL, 10);
            break;
        case 'z': // misalignment
            option->misalign = strtol(optarg, NULL, 10);
            break;
        case 'c': // # of computation operated between each IO
            option->bogus_comp = strtol(optarg, NULL, 10);
            break;
        case 'i': // call usleep(3) for each I/O submission
            option->iosleep = strtol(optarg, NULL, 10);
            break;
        case 'C': // ontinue on error
            option->continue_on_error = true;
            break;
        case 'l': // logfile
            option->logfile_path = strdup(optarg);
            break;
        case 'j': // json print mode
            option->json = true;
            break;
        case 'v': // verbose
            option->verbose = true;
            break;
        default:
            fprintf(stderr, "Unknown option '-%c'\n", optchar);
            goto error;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Device or file is not specified.\n");
        goto error;
    }

    option->nr_files = argc - optind;
    option->file_path_list = malloc(sizeof(char *) * option->nr_files);
    option->file_size_list = malloc(sizeof(int64_t) * option->nr_files);

    for (idx = 0; idx + optind < argc; idx++) {
        int fd;
        char *path;

        path = strdup(argv[optind + idx]);
        option->file_path_list[idx] = path;

        if (option->noop == false) {
            if (option->read) {
                if ((fd = open(path, O_RDONLY)) == -1) {
                    fprintf(stderr, "Cannot open %s with O_RDONLY\n", path);
                    goto error;
                }
                close(fd);
            } else if (option->write) {
                if ((fd = open(path, O_WRONLY)) == -1) {
                    fprintf(stderr, "Cannot open %s with O_WRONLY\n", path);
                    goto error;
                }
                close(fd);
            } else {
                if ((fd = open(path, O_RDWR)) == -1) {
                    fprintf(stderr, "Cannot open %s with O_RDWR\n", path);
                    goto error;
                }
                close(fd);
            }
        }

        int64_t path_sz = mb_getsize(path);
        if (option->blk_sz * option->ofst_start > path_sz){
            fprintf(stderr, "Too big --offset-start. Maximum: %ld\n",
                    path_sz / option->blk_sz);
            goto error;
        }
        if (option->blk_sz * option->ofst_end > path_sz) {
            fprintf(stderr, "Too big --offset-end. Maximum: %ld\n",
                    path_sz / option->blk_sz);
            goto error;
        }

        option->file_size_list[idx] = path_sz;
    }

    if (option->direct && option->blk_sz % 512) {
        fprintf(stderr, "--direct specified. Block size must be multiples of block size of devices.\n");
        goto error;
    }
    if (option->direct && getuid() != 0) {
        fprintf(stderr, "You must be root to use --direct\n");
        goto error;
    }

    // check device

    // aio trace log
    if (option->aio_tracefile != NULL && option->aio == false) {
        fprintf(stderr, "AIO trace log should not be recorded without async mode.\n");
        goto error;
    }

    if (option->logfile_path != NULL) {
        option->logfile = fopen(option->logfile_path, "w");
        if (option->logfile == NULL) {
            fprintf(stderr, "Failed to open log file for write: %s\n", option->logfile_path);
            goto error;
        }
    }

    return 0;

error:
    if (option->file_path_list != NULL) {
        free(option->file_path_list);
        option->file_path_list = NULL;
    }
    if (option->file_size_list != NULL) {
        free(option->file_size_list);
        option->file_size_list = NULL;
    }

    return 1;
}

void
mb_set_option(micbench_io_option_t *option_)
{
    memcpy(&option, option_, sizeof(micbench_io_option_t));
}

static char buf[1024];
static long logcount = 0;
static long log_start_usec;

static void
mb_log_io_activity(struct timeval *issue_tv, struct timeval *complete_tv,
                   const char *file, int64_t blockaddr, int blocksz)
{
    long issue_usec, complete_usec;
    double rt;

    if (option.logfile == NULL) {
        return;
    }

    issue_usec = TVPTR2LONG(issue_tv);
    complete_usec = TVPTR2LONG(complete_tv);
    rt = TVPTR2DOUBLE(complete_tv) - TVPTR2DOUBLE(issue_tv);

    if (logcount == 0) {
        log_start_usec = issue_usec;
    }

    sprintf(buf, "%ld\t%ld\t%ld\t%lf\t%s\t%ld\t%d\n",
            issue_usec, complete_usec, issue_usec - log_start_usec, rt, file, blockaddr, blocksz);

    if (fputs(buf, option.logfile) == EOF) {
        perror("fputs(3) failed.");
    }

    logcount++;
}

void
do_async_io(th_arg_t *arg, int *fd_list)
{
    meter_t *meter;
    struct timeval start_tv;
    int64_t addr;
    int n;
    int i;
    mb_aiom_t *aiom;
    struct drand48_data  rand;

    int file_idx;
    int64_t *ofst_list;
    int64_t *ofst_min_list;
    int64_t *ofst_max_list;

    srand48_r(arg->common_seed ^ arg->tid, &rand);

    meter = arg->meter;
    aiom = mb_aiom_make(option.aio_nr_events);
    if (aiom == NULL) {
        perror("do_async_io:mb_aiom_make failed");
        exit(EXIT_FAILURE);
    }

#ifdef HAVE_IO_URING
    int ret;

    if (option.aio_engine == AIO_IOURING) {
        ret = io_uring_register_files(&aiom->uring, fd_list, option.nr_files);
        if (ret != 0) {
            perror("do_async_io:io_uring_register_files failed");
            exit(EXIT_FAILURE);
        }
    }
#endif

    // offset handling
    ofst_list = malloc(sizeof(int64_t) * option.nr_files);
    ofst_min_list = malloc(sizeof(int64_t) * option.nr_files);
    ofst_max_list = malloc(sizeof(int64_t) * option.nr_files);

    for (i = 0; i < option.nr_files; i++) {
        if (option.ofst_start >= 0) {
            ofst_min_list[i] = option.ofst_start;
        } else {
            ofst_min_list[i] = 0;
        }

        if (option.ofst_end >= 0) {
            ofst_max_list[i] = option.ofst_end;
        } else {
            ofst_max_list[i] = option.file_size_list[i] / option.blk_sz;
        }

        if (option.pattern == PATTERN_SEQ) {
            ofst_list[i] = ofst_min_list[i] + (ofst_max_list[i] - ofst_min_list[i]) * arg->id / option.multi;
        } else {
            ofst_list[i] = ofst_min_list[i];
        }
    }

    if (option.read == false && option.write == false) {
        fprintf(stderr, "Only read or write can be specified in sync. mode\n");
        exit(EXIT_FAILURE);
    }

    file_idx = 0;

    GETTIMEOFDAY(&start_tv);
    while(mb_elapsed_time_from(&start_tv) < option.timeout) {
        while(mb_aiom_nr_submittable(aiom) > 0) {
            // select file
            if (option.pattern == PATTERN_RAND) {
                long ret;
                lrand48_r(&rand, &ret);
                file_idx = ret % option.nr_files;
            } else {
                file_idx++;
                file_idx %= option.nr_files;
            }

            if (option.pattern == PATTERN_RAND) {
                ofst_list[file_idx] = (int64_t) mb_rand_range_long(&rand,
                                                                   ofst_min_list[file_idx],
                                                                   ofst_max_list[file_idx]);
            } else {
                ofst_list[file_idx]++;
                if (ofst_list[file_idx] >= ofst_max_list[file_idx]) {
                    ofst_list[file_idx] = ofst_min_list[file_idx];
                }
            }
            addr = ofst_list[file_idx] * option.blk_sz + option.misalign;

            if (mb_read_or_write() == MB_DO_READ) {
                aiom_cb_t *aiom_cb;
                if (NULL == (aiom_cb = mb_res_pool_pop(aiom->cbpool))) {
                    exit(EXIT_FAILURE);
                }
                mb_aiom_prep_pread(aiom, fd_list[file_idx], file_idx,
                                   aiom_cb, option.blk_sz, addr);
            } else {
                aiom_cb_t *aiom_cb;
                if (NULL == (aiom_cb = mb_res_pool_pop(aiom->cbpool))) {
                    exit(EXIT_FAILURE);
                }
                mb_rand_buf(&rand, aiom_cb->vec->iov_base, option.blk_sz);
                mb_aiom_prep_pwrite(aiom, fd_list[file_idx], file_idx,
                                    aiom_cb, option.blk_sz, addr);
            }
        }
        mb_aiom_submit(aiom);

        if (0 >= (n = mb_aiom_wait(aiom, NULL))) {
            perror("do_async_io:mb_aiom_wait failed");
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < n; i++) {
            /* do bogus comp after I/O completion */
            long idx;
            volatile double dummy = 0.0;
            for(idx = 0; idx < option.bogus_comp; idx++){
                dummy += idx;
            }
            if (option.iosleep > 0) {
                usleep(option.iosleep);
            }
        }
    }

    mb_aiom_waitall(aiom);

    meter->count = aiom->iocount;
    meter->iowait_time = aiom->iowait;

    mb_aiom_destroy(aiom);

    free(ofst_list);
    free(ofst_min_list);
    free(ofst_max_list);
}

void
do_sync_io(th_arg_t *th_arg, int *fd_list)
{
    struct timeval       start_tv;
    struct timeval       t0;
    struct timeval       t1;
    meter_t             *meter;
    struct drand48_data  rand;
    int64_t              addr;
    void                *buf;
    int                  i;

    int file_idx;
    int64_t *ofst_list;
    int64_t *ofst_min_list;
    int64_t *ofst_max_list;
    int64_t *seekdist_side_list; // 0:lower LBA side, 1:upper LBA side

    meter = th_arg->meter;
    srand48_r(th_arg->common_seed ^ th_arg->tid, &rand);

    register double iowait_time = 0;
    register int64_t io_count = 0;

    buf = memalign(option.blk_sz, option.blk_sz);

    // offset handling
    ofst_list = malloc(sizeof(int64_t) * option.nr_files);
    ofst_min_list = malloc(sizeof(int64_t) * option.nr_files);
    ofst_max_list = malloc(sizeof(int64_t) * option.nr_files);
    seekdist_side_list = malloc(sizeof(int64_t) * option.nr_files);

    for (i = 0; i < option.nr_files; i++) {
        if (option.ofst_start >= 0) {
            ofst_min_list[i] = option.ofst_start;
        } else {
            ofst_min_list[i] = 0;
        }

        if (option.ofst_end >= 0) {
            ofst_max_list[i] = option.ofst_end;
        } else {
            ofst_max_list[i] = option.file_size_list[i] / option.blk_sz;
        }

        if (option.pattern == PATTERN_SEQ) {
            ofst_list[i] = ofst_min_list[i]
                + (ofst_max_list[i] - ofst_min_list[i]) * th_arg->id / option.multi;
        } else if (option.pattern == PATTERN_SEEKDIST) {
            ofst_list[i] = ofst_max_list[i] - option.seekdist_stride;
        } else {
            ofst_list[i] = ofst_min_list[i];
        }

        if (option.pattern == PATTERN_SEEKDIST) {
            seekdist_side_list[i] = 0;
        }
    }

    file_idx = 0;

    GETTIMEOFDAY(&start_tv);
    if (option.pattern == PATTERN_RAND){
        while (mb_elapsed_time_from(&start_tv) < option.timeout) {
            for(i = 0;i < 100; i++){
                // select file
                long ret;
                lrand48_r(&rand, &ret);
                file_idx = ret % option.nr_files;

                ofst_list[file_idx] = (int64_t) mb_rand_range_long(&rand,
                                                                   ofst_min_list[file_idx],
                                                                   ofst_max_list[file_idx]);

                addr = ofst_list[file_idx] * option.blk_sz + option.misalign;

                GETTIMEOFDAY(&t0);
                if (mb_read_or_write() == MB_DO_READ) {
                    mb_preadall(fd_list[file_idx], buf, option.blk_sz, addr, option.continue_on_error);
                } else {
                    mb_rand_buf(&rand, buf, option.blk_sz);
                    mb_pwriteall(fd_list[file_idx], buf, option.blk_sz, addr, option.continue_on_error);
                }
                GETTIMEOFDAY(&t1);
                iowait_time += (TV2LONG(t1) - TV2LONG(t0))/1.0e6;
                io_count ++;

                mb_log_io_activity(&t0, &t1, option.file_path_list[file_idx], addr, option.blk_sz);

                long idx;
                volatile double dummy = 0.0;
                for(idx = 0; idx < option.bogus_comp; idx++){
                    dummy += idx;
                }
                if (option.iosleep > 0) {
                    usleep(option.iosleep);
                }
            }
        }
    } else if (option.pattern == PATTERN_SEQ) {
        while (mb_elapsed_time_from(&start_tv) < option.timeout) {
            for(i = 0;i < 100; i++){
                // select file
                file_idx++;
                file_idx %= option.nr_files;

                // incr offset
                ofst_list[file_idx]++;
                if (ofst_list[file_idx] >= ofst_max_list[file_idx]) {
                    ofst_list[file_idx] = ofst_min_list[file_idx];
                }
                addr = ofst_list[file_idx] * option.blk_sz + option.misalign;
                if (lseek64(fd_list[file_idx], addr, SEEK_SET) == -1){
                    perror("do_sync_io:lseek64");
                    exit(EXIT_FAILURE);
                }

                GETTIMEOFDAY(&t0);
                if (option.read) {
                    mb_readall(fd_list[file_idx], buf, option.blk_sz, option.continue_on_error);
                } else if (option.write) {
                    mb_rand_buf(&rand, buf, option.blk_sz);
                    mb_writeall(fd_list[file_idx], buf, option.blk_sz, option.continue_on_error);
                } else {
                    fprintf(stderr, "Only read or write can be specified in seq.");
                    exit(EXIT_FAILURE);
                }
                GETTIMEOFDAY(&t1);
                iowait_time += (TV2LONG(t1) - TV2LONG(t0))/1.0e6;
                io_count ++;

                mb_log_io_activity(&t0, &t1, option.file_path_list[file_idx], addr, option.blk_sz);

                long idx;
                volatile double dummy = 0.0;
                for(idx = 0; idx < option.bogus_comp; idx++){
                    dummy += idx;
                }
                if (option.iosleep > 0) {
                    usleep(option.iosleep);
                }
            }
        }
    } else if (option.pattern == PATTERN_SEEKDIST) {
        while (mb_elapsed_time_from(&start_tv) < option.timeout) {
            for(i = 0;i < 100; i++){
                int64_t ofst;

                // select file
                file_idx++;
                file_idx %= option.nr_files;

                // change offset

                if (seekdist_side_list[file_idx] == 0) {
                    // move to upper LBA side
                    ofst =
                        (int64_t) mb_rand_range_long(&rand,
                                                     ofst_max_list[file_idx] - option.seekdist_stride,
                                                     ofst_max_list[file_idx]);
                } else {
                    // move to lower LBA side
                    ofst =
                        (int64_t) mb_rand_range_long(&rand,
                                                     ofst_min_list[file_idx],
                                                     ofst_min_list[file_idx] + option.seekdist_stride);
                }
                seekdist_side_list[file_idx] ^= 1;

                addr = ofst * option.blk_sz + option.misalign;

                // printf("%d\t%ld\n", file_idx, addr);

                if (lseek64(fd_list[file_idx], addr, SEEK_SET) == -1){
                    perror("do_sync_io:lseek64");
                    exit(EXIT_FAILURE);
                }

                GETTIMEOFDAY(&t0);
                if (option.read) {
                    mb_readall(fd_list[file_idx], buf, option.blk_sz, option.continue_on_error);
                } else if (option.write) {
                    mb_rand_buf(&rand, buf, option.blk_sz);
                    mb_writeall(fd_list[file_idx], buf, option.blk_sz, option.continue_on_error);
                } else {
                    fprintf(stderr, "Only read or write can be specified in seq.");
                    exit(EXIT_FAILURE);
                }
                GETTIMEOFDAY(&t1);
                iowait_time += (TV2LONG(t1) - TV2LONG(t0))/1.0e6;
                io_count ++;

                mb_log_io_activity(&t0, &t1, option.file_path_list[file_idx], addr, option.blk_sz);

                long idx;
                volatile double dummy = 0.0;
                for(idx = 0; idx < option.bogus_comp; idx++){
                    dummy += idx;
                }
                if (option.iosleep > 0) {
                    usleep(option.iosleep);
                }
            }
        }
    }

    meter->iowait_time = iowait_time;
    meter->count = io_count;

    free(buf);
}

void *
thread_handler(void *arg)
{
    th_arg_t *th_arg = (th_arg_t *) arg;
    mb_affinity_t *aff;
    int i;
    int fd;
    int *fd_list;

    tid = syscall(SYS_gettid);
    th_arg->tid = tid;

    if (option.affinities != NULL){
        aff = option.affinities[th_arg->id];
        if (aff != NULL){
            sched_setaffinity(tid,
                              sizeof(cpu_set_t),
                              &aff->cpumask);
        }
    }

    fd_list = malloc(sizeof(int) * option.nr_files);
    for (i = 0; i < option.nr_files; i++) {
        if ((fd = open(option.file_path_list[i], option.open_flags)) == -1){
            perror("main:open(2)");
            exit(EXIT_FAILURE);
        }
        fd_list[i] = fd;
    }

    if (option.aio == true) {
        if (option.verbose) fprintf(stderr, "*info* do_async_io\n");
        do_async_io(th_arg, fd_list);
    } else {
        if (option.verbose) fprintf(stderr, "*info* do_sync_io\n");
        do_sync_io(th_arg, fd_list);
    }

    for (i = 0; i < option.nr_files; i++) {
        if (close(fd_list[i]) == -1){
            perror("main:close(2)");
            exit(EXIT_FAILURE);
        }
    }


    return NULL;
}

int
micbench_io_main(int argc, char **argv)
{
    th_arg_t *th_args;
    int i;
    int flags;
    struct timeval start_tv;
    result_t result;
    meter_t *meter;
    long common_seed;
    double exec_time;

    if (getenv("MICBENCH") == NULL) {
        fprintf(stderr, "Variable MICBENCH is not set.\n"
                "This process should be invoked via \"micbench\" command.\n");
        exit(EXIT_FAILURE);
    }

    if (parse_args(argc, argv, &option) != 0) {
        fprintf(stderr, "Argument Error.\n");
        exit(EXIT_FAILURE);
    }

    if (option.noop == true){
        print_result_json(NULL, true);
        exit(EXIT_SUCCESS);
    }

    th_args = malloc(sizeof(th_arg_t) * option.multi);

    if (option.read) {
        flags = O_RDONLY;
    } else if (option.write) {
        flags = O_WRONLY;
    } else {
        flags = O_RDWR;
    }
    if (option.direct) {
        flags |= O_DIRECT;
    }
    option.open_flags = flags;


    // initialize common seed value with /dev/urandom
    FILE *f;
    f = fopen("/dev/urandom", "r");
    if (f == NULL) {
        perror("Failed to open /dev/urandom.\n");
        exit(EXIT_FAILURE);
    }
    if (fread(&common_seed, sizeof(common_seed), 1, f) == 0) {
        perror("fread failed to read /dev/urandom.\n");
        exit(EXIT_FAILURE);
    }
    fclose(f);

    if (option.aio_tracefile != NULL) {
        aio_tracefile = fopen(option.aio_tracefile, "w");
    } else {
        aio_tracefile = NULL;
    }

    for(i = 0;i < option.multi;i++){
        th_args[i].id          = i;
        th_args[i].self        = malloc(sizeof(pthread_t));
        th_args[i].common_seed = common_seed;
        meter              = th_args[i].meter = malloc(sizeof(meter_t));
        meter->iowait_time = 0;
        meter->count       = 0;
    }

    GETTIMEOFDAY(&start_tv);
    for(i = 0;i < option.multi;i++){
        pthread_create(th_args[i].self, NULL, thread_handler, &th_args[i]);
    }

    for(i = 0;i < option.multi;i++){
        pthread_join(*th_args[i].self, NULL);
    }
    exec_time = mb_elapsed_time_from(&start_tv);

    int64_t count_sum = 0;
    double iowait_time_sum = 0;
    result.io_count = 0;
    result.io_bytes = 0;

    for(i = 0;i < option.multi;i++){
        meter = th_args[i].meter;
        count_sum += meter->count;
        iowait_time_sum += meter->iowait_time;
    }

    result.io_count = count_sum;
    result.io_bytes = count_sum * option.blk_sz;

    result.start_time = TV2LONG(start_tv) / 1.0e6;
    result.exec_time = exec_time;
    result.iowait_time = iowait_time_sum / option.multi;
    result.response_time = iowait_time_sum / count_sum;
    result.iops = count_sum / result.exec_time;
    result.bandwidth = count_sum * option.blk_sz / result.exec_time;

    if (option.json) {
        print_result_json(&result, false);
    } else {
        print_result(&result);
    }

    for(i = 0;i < option.multi;i++){
        free(th_args[i].meter);
        free(th_args[i].self);
    }
    free(th_args);

    if (option.affinities != NULL){
        for(i = 0; i < option.multi; i++){
            if (option.affinities[i] != NULL){
                free(option.affinities[i]);
            }
        }
        free(option.affinities);
    }

    return 0;
}
