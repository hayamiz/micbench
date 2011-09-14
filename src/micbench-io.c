#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE

#include "micbench-io.h"

typedef struct {
    pid_t thread_id;
    cpu_set_t mask;
    unsigned long nodemask;
} thread_assignment_t;

static micbench_io_option_t option;

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


int64_t
getsize(const char *path)
{
    int fd;
    int64_t size;
    struct stat statbuf;

    size = -1;

    if ((fd = open(path, O_RDONLY)) == -1){
        perror("Failed to open device or file\n");
        return -1;
    }
    if (fstat(fd, &statbuf) == -1){
        perror("fstat(2) failed.\n");
        goto finally;
    }

    if (S_ISREG(statbuf.st_mode)){
        size = statbuf.st_size;
        goto finally;
    }

    if (S_ISBLK(statbuf.st_mode)){
        uint64_t dev_sz;
        // if(ioctl(fd, BLKGETSIZE, &blk_num) == -1){
        //     perror("ioctl(BLKGETSIZE) failed\n");
        //     goto finally;
        // }
        // if(ioctl(fd, BLKSSZGET, &blk_sz) == -1){
        //     perror("ioctl(BLKSSZGET) failed\n");
        //     goto finally;
        // }
        if (ioctl(fd, BLKGETSIZE64, &dev_sz) == -1){
            perror("ioctl(BLKGETSIZE64) failed\n");
            goto finally;
        }

        // size = blk_num * blk_sz; // see <linux/fs.h>
        size = dev_sz;
    }

finally:
    close(fd);
    return size;
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
offset_start    %ld\n\
offset_end      %ld\n\
misalign        %ld\n\
",
            option.multi,
            option.path,
            (option.seq ? "sequential" : "random"),
            (option.read ? "read" : "write"),
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
iops          %lf [blocks/sec]\n\
response_time %lf [sec]\n\
transfer_rate %lf [MiB/sec]\n\
accum_io_time %lf [sec]\n\
",
            result->iops,
            result->response_time,
            result->bandwidth / MEBI,
            result->iowait_time);
}

void
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
    option->seq = true;
    option->rand = false;
    option->direct = false;
    option->blk_sz = 64 * KIBI;
    option->ofst_start = 0;
    option->ofst_end = 0;
    option->misalign = 0;
    option->verbose = false;

    optind = 1;
    while ((optchar = getopt(argc, argv, "+Nm:a:t:RSdWb:s:e:z:c:v")) != -1){
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
                    exit(EXIT_FAILURE);
                }
            }
            if (option->affinities == NULL){
                option->affinities = malloc(sizeof(mb_affinity_t *) * option->multi);
                bzero(option->affinities, sizeof(mb_affinity_t *) * option->multi);
            }
            if ((aff = mb_parse_affinity(NULL, optarg)) == NULL){
                fprintf(stderr, "Invalid argument for -a: %s\n", optarg);
                exit(EXIT_FAILURE);
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
        case 'W': // write
            option->write = true;
            option->read = false;
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
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Device or file is not specified.\n");
        exit(EXIT_FAILURE);
    }
    option->path = argv[optind];

    // check device

    if (option->noop == false) {
        if (option->read) {
            if (open(option->path, O_RDONLY) == -1) {
                fprintf(stderr, "Cannot open %s with O_RDONLY\n", option->path);
                goto error;
            }
        } else {
            if (open(option->path, O_WRONLY) == -1) {
                fprintf(stderr, "Cannot open %s with O_WRONLY\n", option->path);
                goto error;
            }
        }
    }

    int64_t path_sz = getsize(option->path);
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

    return;
error:
    exit(EXIT_FAILURE);
}

static inline ssize_t
iostress_readall(int fd, char *buf, size_t size)
{
    size_t sz = size;
    ssize_t ret;

    while(true) {
        if ((ret = read(fd, buf, sz)) == -1){
            printf("fd=%d, buf=%p, sz=%ld\n", fd, buf, sz);
            perror("iostress_readall:read");
            exit(EXIT_FAILURE);
        }
    
        if (ret < sz) {
            sz -= ret;
            buf += ret;
        } else {
            break;
        }
    }

    return size;
}

static inline ssize_t
iostress_writeall(int fd, const char *buf, size_t size)
{
    size_t sz = size;
    ssize_t ret;

    while(true) {
        if ((ret = write(fd, buf, sz)) == -1){
            perror("iostress_writeall:write");
            exit(EXIT_FAILURE);
        }
    
        if (ret < sz) {
            sz -= ret;
            buf += ret;
        } else {
            break;
        }
    }

    return size;
}

void
do_iostress(th_arg_t *th_arg)
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
                if (lseek64(fd, addr, SEEK_SET) == -1){
                    perror("do_iostress:lseek64");
                    exit(EXIT_FAILURE);
                }

                GETTIMEOFDAY(&timer);
                if (option.read) {
                    iostress_readall(fd, buf, option.blk_sz);
                } else if (option.write) {
                    iostress_writeall(fd, buf, option.blk_sz);
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
            perror("do_iostress:lseek64");
            exit(EXIT_FAILURE);
        }

        while (mb_elapsed_time_from(&start_tv) < option.timeout) {
            for(i = 0;i < 100; i++){
                GETTIMEOFDAY(&timer);
                if (option.read) {
                    iostress_readall(fd, buf, option.blk_sz);
                } else if (option.write) {
                    iostress_writeall(fd, buf, option.blk_sz);
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
                        perror("do_iostress:lseek64");
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

    if (option.affinities != NULL){
        aff = option.affinities[th_arg->id];
        if (aff != NULL){
            sched_setaffinity(syscall(SYS_gettid),
                              sizeof(cpu_set_t),
                              &aff->cpumask);
        }
    }

    do_iostress(th_arg);

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

    parse_args(argc, argv, &option);

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
    } else {
        flags = O_WRONLY;
    }
    if (option.direct) {
        flags |= O_DIRECT;
    }

    common_seed = time(NULL);
    for(i = 0;i < option.multi;i++){
        if ((fd = open(option.path, flags)) == -1){
            perror("main:open(2)");
            exit(EXIT_FAILURE);
        }

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
