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
    double exec_time;
    double iowait_time;
    int64_t count;
    double response_time;
    double iops;
    double bandwidth;
} result_t;

typedef struct {
    int id;
    pthread_t *self;
    meter_t *meter;
    long common_seed;

    int fd;
} th_arg_t;



mb_aiom_t *
mb_aiom_make(int nr_events)
{
    mb_aiom_t *aiom;
    int i;
    struct iocb *iocb;
    int ret;

    aiom = malloc(sizeof(mb_aiom_t));

    aiom->nr_events = nr_events;
    aiom->nr_pending = 0;
    aiom->nr_inflight = 0;

    aiom->iocount = 0;

    aiom->pending = malloc(sizeof(struct iocb *) * nr_events);
    bzero(aiom->pending, sizeof(struct iocb *) * nr_events);

    aiom->events = malloc(sizeof(struct io_event) * nr_events);
    bzero(aiom->events, sizeof(struct io_event) * nr_events);

    aiom->cbpool = mb_res_pool_make(nr_events);


    for(i = 0; i < nr_events; i++) {
        iocb = malloc(sizeof(struct iocb));
        bzero(iocb, sizeof(struct iocb));
        mb_res_pool_push(aiom->cbpool, iocb);
    }

    bzero(&aiom->context, sizeof(io_context_t));
    if ((ret = io_setup(nr_events, &aiom->context)) != 0) {
        return NULL;
    }

    return aiom;
}

void
mb_aiom_destroy (mb_aiom_t *aiom)
{
    struct iocb *iocb;

    free(aiom->pending);
    free(aiom->events);
    io_destroy(aiom->context);
    while((iocb = mb_res_pool_pop(aiom->cbpool)) != NULL) {
        free(iocb);
    }
    mb_res_pool_destroy(aiom->cbpool);
    free(aiom);
}

int
mb_aiom_submit(mb_aiom_t *aiom)
{
    int ret;

    if (aiom->nr_pending == 0) {
        return 0;
    }

    ret = io_submit(aiom->context, aiom->nr_pending, aiom->pending);

    if (aio_tracefile != NULL) {
        fprintf(aio_tracefile,
                "[%d] submit: %d req\n",
                tid, ret);
    }

    if (ret != aiom->nr_pending) {
        fprintf(stderr, "fatal error\n");
        exit(EXIT_FAILURE);
    }

    aiom->nr_inflight += aiom->nr_pending;
    aiom->nr_pending = 0;

    return ret;
}

struct iocb *
mb_aiom_prep_pread   (mb_aiom_t *aiom, int fd,
                      void *buf, size_t count, long long offset)
{
    struct iocb *iocbp;
    if (NULL == (iocbp = mb_res_pool_pop(aiom->cbpool))) {
        return NULL;
    }
    io_prep_pread(iocbp, fd, buf, count, offset);

    aiom->pending[aiom->nr_pending++] = iocbp;

    return iocbp;
}

struct iocb *
mb_aiom_prep_pwrite  (mb_aiom_t *aiom, int fd,
                      void *buf, size_t count, long long offset)
{
    struct iocb *iocbp;
    if (NULL == (iocbp = mb_res_pool_pop(aiom->cbpool))) {
        return NULL;
    }
    io_prep_pwrite(iocbp, fd, buf, count, offset);

    aiom->pending[aiom->nr_pending++] = iocbp;

    return iocbp;
}

int
mb_aiom_submit_pread (mb_aiom_t *aiom, int fd, void *buf, size_t count, long long offset)
{
    mb_aiom_prep_pread(aiom, fd, buf, count, offset);
    return mb_aiom_submit(aiom);
}

int
mb_aiom_submit_pwrite(mb_aiom_t *aiom, int fd, void *buf, size_t count, long long offset)
{
    mb_aiom_prep_pwrite(aiom, fd, buf, count, offset);
    return mb_aiom_submit(aiom);
}

int
__mb_aiom_getevents(mb_aiom_t *aiom, long min_nr, long nr,
                    struct io_event *events, struct timespec *timeout){
    int i;
    int nr_completed;
    struct iocb *iocb;
    struct io_event *event;

    nr_completed = io_getevents(aiom->context, min_nr, nr, events, timeout);
    aiom->nr_inflight -= nr_completed;
    aiom->iocount += nr_completed;

    if (aio_tracefile != NULL) {
        fprintf(aio_tracefile,
                "[%d] %d infl %d comp\n",
                tid, aiom->nr_inflight, nr_completed);
    }

    for(i = 0; i < nr_completed; i++){
        event = &aiom->events[i];
        iocb = event->obj;

        // TODO: callback or something
        if (!(event->res == option.blk_sz && event->res2 == 0)){
            fprintf(stderr, "fatal error: res = %ld, res2 = %ld\n",
                    (long) event->res, (long) event->res2);
        }

        mb_res_pool_push(aiom->cbpool, iocb);
    }

    return nr_completed;
}

int
mb_aiom_wait(mb_aiom_t *aiom, struct timespec *timeout)
{
    return __mb_aiom_getevents(aiom, 1, aiom->nr_inflight, aiom->events, timeout);
}

int
mb_aiom_waitall(mb_aiom_t *aiom)
{
    return __mb_aiom_getevents(aiom, aiom->nr_inflight, aiom->nr_inflight,
                               aiom->events, NULL);
}

int
mb_aiom_nr_submittable(mb_aiom_t *aiom)
{
    return aiom->cbpool->nr_avail;
}

mb_res_pool_t *
mb_res_pool_make(int nr_events)
{
    mb_res_pool_t *pool;
    mb_res_pool_cell_t *next_cell;
    mb_res_pool_cell_t *cell;
    int i;

    pool = malloc(sizeof(mb_res_pool_t));
    bzero(pool, sizeof(mb_res_pool_t));

    pool->size = nr_events;
    pool->nr_avail = 0;

    for(cell = NULL, i = 0; i < nr_events; i++) {
        next_cell = cell;
        cell = malloc(sizeof(mb_res_pool_cell_t));
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
struct iocb *
mb_res_pool_pop(mb_res_pool_t *pool)
{
    struct iocb *ret;
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
mb_res_pool_push(mb_res_pool_t *pool, struct iocb *iocb)
{
    if (pool->nr_avail == pool->size) {
        return -1;
    }
    pool->tail = pool->tail->next;
    pool->tail->data = iocb;
    pool->nr_avail ++;

    return 0;
}


void
print_option()
{
    fprintf(stderr, "== configuration summary ==\n\
multiplicity    %d\n\
device_or_file  %s\n\
access_pattern  %s\n\
access_mode     %s\n\
direct_io       %s\n\
timeout         %d\n\
bogus_comp		%ld\n\
block_size      %d\n\
offset_start    %lld\n\
offset_end      %lld\n\
misalign        %lld\n\
",
            option.multi,
            option.path,
            (option.seq ? "sequential" : "random"),
            (option.read ? "read" :
             option.write ? "write" : "mix"),
            (option.direct ? "yes" : "no"),
            option.timeout,
            option.bogus_comp,
            option.blk_sz,
            option.ofst_start,
            option.ofst_end,
            option.misalign);
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
    option->read = true;
    option->write = false;
    option->rwmix = 0.0;
    option->seq = true;
    option->rand = false;
    option->direct = false;
    option->aio = false;
    option->aio_nr_events = 64;
    option->aio_tracefile = NULL;
    option->blk_sz = 64 * KIBI;
    option->ofst_start = 0;
    option->ofst_end = 0;
    option->misalign = 0;
    option->verbose = false;

    optind = 1;
    while ((optchar = getopt(argc, argv, "+Nm:a:t:RSdAE:T:WM:b:s:e:z:c:v")) != -1){
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
            option->rand = true;
            option->seq = false;
            break;
        case 'S': // sequential
            option->seq = true;
            option->rand = false;
            break;
        case 'd': // direct IO
            option->direct = true;
            break;
        case 'A': // Asynchronous IO
            option->aio = true;
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
        case 'z': // misalignment
            option->misalign = strtol(optarg, NULL, 10);
            break;
        case 'c': // # of computation operated between each IO
            option->bogus_comp = strtol(optarg, NULL, 10);
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
    option->path = argv[optind];

    // check device

    int fd;
    if (option->noop == false) {
        if (option->read) {
            if ((fd = open(option->path, O_RDONLY)) == -1) {
                fprintf(stderr, "Cannot open %s with O_RDONLY\n", option->path);
                goto error;
            }
            close(fd);
        } else if (option->write) {
            if ((fd = open(option->path, O_WRONLY)) == -1) {
                fprintf(stderr, "Cannot open %s with O_WRONLY\n", option->path);
                goto error;
            }
            close(fd);
        } else {
            if ((fd = open(option->path, O_RDWR)) == -1) {
                fprintf(stderr, "Cannot open %s with O_RDWR\n", option->path);
                goto error;
            }
            close(fd);
        }
    }

    int64_t path_sz = mb_getsize(option->path);
    if (option->blk_sz * option->ofst_start > path_sz){
        fprintf(stderr, "Too big --offset-start. Maximum: %lld\n",
                   path_sz / option->blk_sz);
        goto error;
    }
    if (option->blk_sz * option->ofst_end > path_sz) {
        fprintf(stderr, "Too big --offset-end. Maximum: %lld\n",
                   path_sz / option->blk_sz);
        goto error;
    }
    if (option->direct && option->blk_sz % 512) {
        fprintf(stderr, "--direct specified. Block size must be multiples of block size of devices.\n");
        goto error;
    }
    if (option->direct && getuid() != 0) {
        fprintf(stderr, "You must be root to use --direct\n");
        goto error;
    }
    if (option->ofst_end == 0) {
        option->ofst_end = path_sz / option->blk_sz;
    }

    // aio trace log
    if (option->aio_tracefile != NULL && option->aio == false) {
        fprintf(stderr, "AIO trace log should not be recorded without async mode.\n");
        goto error;
    }

    return 0;
error:
    return 1;
}

void
mb_set_option(micbench_io_option_t *option_)
{
    memcpy(&option, option_, sizeof(micbench_io_option_t));
}

void
do_async_io(th_arg_t *arg)
{
    int fd;
    meter_t *meter;
    struct timeval start_tv;
    int64_t ofst;
    int64_t addr;
    int n;
    int i;
    void *buf;
    mb_aiom_t *aiom;
    mb_res_pool_t *buffer_pool;

    fd = arg->fd;
    meter = arg->meter;
    aiom = mb_aiom_make(option.aio_nr_events);
    if (aiom == NULL) {
        perror("do_async_io:mb_aiom_make failed");
        exit(EXIT_FAILURE);
    }
    buf = malloc(option.blk_sz);
    bzero(buf, option.blk_sz);
    buffer_pool = mb_res_pool_make(option.aio_nr_events);
    for(i = 0; i < option.aio_nr_events; i++){
        buf = memalign(512, option.blk_sz);
        mb_res_pool_push(buffer_pool, buf);
    }

    if (option.seq) {
        ofst = option.ofst_start + (option.ofst_end - option.ofst_start) * arg->id / option.multi;
    } else {
        ofst = 0;
    }

    if (option.read == false && option.write == false) {
        fprintf(stderr, "Only read or write can be specified in seq.");
        exit(EXIT_FAILURE);
    }

    GETTIMEOFDAY(&start_tv);
    while(mb_elapsed_time_from(&start_tv) < option.timeout) {
        while(mb_aiom_nr_submittable(aiom) > 0) {
            if (option.rand) {
                ofst = (int64_t) mb_rand_range_long(option.ofst_start,
                                                    option.ofst_end);
            }
            addr = ofst * option.blk_sz + option.misalign;

            buf = mb_res_pool_pop(buffer_pool);
            if (mb_read_or_write() == MB_DO_READ) {
                mb_aiom_prep_pread(aiom, fd, buf, option.blk_sz, ofst * option.blk_sz);
            } else {
                mb_aiom_prep_pwrite(aiom, fd, buf, option.blk_sz, ofst * option.blk_sz);
            }
            ofst++;
        }
        if (0 >= (n = mb_aiom_submit(aiom))) {
            perror("do_async_io:mb_aiom_submit failed");
            switch(-n){
            case EAGAIN : fprintf(stderr, "EAGAIN\n"); break;
            case EBADF  : fprintf(stderr, "EBADF\n"); break;
            case EFAULT : fprintf(stderr, "EFAULT\n"); break;
            case EINVAL : fprintf(stderr, "EINVAL\n"); break;
            case ENOSYS : fprintf(stderr, "ENOSYS\n"); break;
            }
            exit(EXIT_FAILURE);
        }

        if (0 >= (n = mb_aiom_wait(aiom, NULL))) {
            perror("do_async_io:mb_aiom_wait failed");
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < n; i++) {
            struct io_event *event;
            event = &aiom->events[i];

            mb_res_pool_push(buffer_pool, event->obj->u.c.buf);
        }
    }

    mb_aiom_waitall(aiom);

    meter->count += aiom->iocount;

    mb_aiom_destroy(aiom);
}

void
do_sync_io(th_arg_t *th_arg)
{
    struct timeval       start_tv;
    struct timeval       timer;
    meter_t             *meter;
    struct drand48_data  rand;
    int                  fd;
    int64_t              ofst;
    int64_t              addr;
    void                *buf;
    int                  i;

    fd = th_arg->fd;
    meter = th_arg->meter;
    ofst = 0;
    srand48_r(th_arg->common_seed + th_arg->id, &rand);

    register double iowait_time = 0;
    register int64_t io_count = 0;

    buf = memalign(option.blk_sz, option.blk_sz);

    GETTIMEOFDAY(&start_tv);
    if (option.rand){
        while (mb_elapsed_time_from(&start_tv) < option.timeout) {
            for(i = 0;i < 100; i++){
                ofst = (int64_t) mb_rand_range_long(option.ofst_start,
                                                    option.ofst_end);
                addr = ofst * option.blk_sz + option.misalign;

                GETTIMEOFDAY(&timer);
                if (mb_read_or_write() == MB_DO_READ) {
                    mb_preadall(fd, buf, option.blk_sz, addr);
                } else {
                    mb_pwriteall(fd, buf, option.blk_sz, addr);
                }
                iowait_time += mb_elapsed_time_from(&timer);
                io_count ++;

                long idx;
                volatile double dummy = 0.0;
                for(idx = 0; idx < option.bogus_comp; idx++){
                    dummy += idx;
                }
            }
        }
    } else if (option.seq) {
        ofst = option.ofst_start + ((option.ofst_end - option.ofst_start) * th_arg->id) / option.multi;
        addr = ofst * option.blk_sz + option.misalign;
        if (lseek64(fd, addr, SEEK_SET) == -1){
            perror("do_sync_io:lseek64");
            exit(EXIT_FAILURE);
        }

        while (mb_elapsed_time_from(&start_tv) < option.timeout) {
            for(i = 0;i < 100; i++){
                GETTIMEOFDAY(&timer);
                if (option.read) {
                    mb_readall(fd, buf, option.blk_sz);
                } else if (option.write) {
                    mb_writeall(fd, buf, option.blk_sz);
                } else {
                    fprintf(stderr, "Only read or write can be specified in seq.");
                    exit(EXIT_FAILURE);
                }
                iowait_time += mb_elapsed_time_from(&timer);
                io_count ++;

                long idx;
                volatile double dummy = 0.0;
                for(idx = 0; idx < option.bogus_comp; idx++){
                    dummy += idx;
                }

                ofst ++;
                if (ofst >= option.ofst_end) {
                    ofst = option.ofst_start;
                    addr = ofst * option.blk_sz + option.misalign;
                    if (lseek64(fd, addr, SEEK_SET) == -1){
                        perror("do_sync_io:lseek64");
                        exit(EXIT_FAILURE);
                    }
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

    tid = syscall(SYS_gettid);

    if (option.affinities != NULL){
        aff = option.affinities[th_arg->id];
        if (aff != NULL){
            sched_setaffinity(tid,
                              sizeof(cpu_set_t),
                              &aff->cpumask);
        }
    }

    if (option.aio == true) {
        if (option.verbose) fprintf(stderr, "do_async_io\n");
        do_async_io(th_arg);
    } else {
        if (option.verbose) fprintf(stderr, "do_sync_io\n");
        do_sync_io(th_arg);
    }

    return NULL;
}

int
micbench_io_main(int argc, char **argv)
{
    th_arg_t *th_args;
    int i;
    int fd = 0;
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
        print_option();
        exit(EXIT_SUCCESS);
    }
    if (option.verbose){
        print_option();
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

    common_seed = time(NULL);

    if ((fd = open(option.path, flags)) == -1){
        perror("main:open(2)");
        exit(EXIT_FAILURE);
    }

    if (option.aio_tracefile != NULL) {
        aio_tracefile = fopen(option.aio_tracefile, "w");
    } else {
        aio_tracefile = NULL;
    }

    for(i = 0;i < option.multi;i++){
        th_args[i].id          = i;
        th_args[i].self        = malloc(sizeof(pthread_t));
        th_args[i].fd          = fd;
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
    close(fd);

    int64_t count_sum = 0;
    double iowait_time_sum = 0;
    for(i = 0;i < option.multi;i++){
        meter = th_args[i].meter;
        count_sum += meter->count;
        iowait_time_sum += meter->iowait_time;
    }

    result.exec_time = exec_time;
    result.iowait_time = iowait_time_sum / option.multi;
    result.response_time = iowait_time_sum / count_sum;
    result.iops = count_sum / result.exec_time;
    result.bandwidth = count_sum * option.blk_sz / result.exec_time;

    print_result(&result);

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
