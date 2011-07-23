
#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <ctype.h>

#include <pthread.h>
#include <malloc.h>
#include <sched.h>

#include <linux/fs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

#include "micbench-utils.h"

#define NPROCESSOR (sysconf(_SC_NPROCESSORS_ONLN))

typedef struct {
    pid_t thread_id;
    cpu_set_t mask;
    unsigned long nodemask;
} thread_assignment_t;

static struct {
    // multiplicity of IO
    int multi;

    // access mode
    bool seq;
    bool rand;
    bool direct;
    bool read;
    bool write;

    // thread affinity assignment
    const char *assignment_str;
    thread_assignment_t **assigns;

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

    bool verbose;

    bool noop;
} option;

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
        return -1;
    }
    if (fstat(fd, &statbuf) == -1){
        goto finally;
    }

    if (S_ISREG(statbuf.st_mode)){
        size = statbuf.st_size;
        goto finally;
    }

    if (S_ISBLK(statbuf.st_mode)){
        int blk_num;
        int blk_sz;
        uint64_t dev_sz;
        if(ioctl(fd, BLKGETSIZE, &blk_num) == -1){
            goto finally;
        }
        if(ioctl(fd, BLKSSZGET, &blk_sz) == -1){
            goto finally;
        }
        if (ioctl(fd, BLKGETSIZE64, &dev_sz)){
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
thread affinity	%s\n\
timeout         %d\n\
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
               option.assignment_str,
               option.timeout,
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

/*
 * Thread affinity specification
 *
 * <assignments> := <assignment> (',' <assignments>)*
 * <assignment> := <threads> ':' <physical_core_id> [':' <mem_node_id>]
 * <threads> := <thread_id> | <thread_id> '-' <thread_id>
 * <thread_id> := [1-9][0-9]* | 0
 * <physical_core_id> := [1-9][0-9]* | 0
 * <mem_mode> := [1-9][0-9]* | 0
 *
 */
int
parse_thread_assignment(int num_threads, thread_assignment_t **assigns, const char *assignment_str)
{
    int thread_id;
    int thread_id_end;
    int core_id;
    int mem_node_id;
    int i;

    const char *str;
    char *endptr;

    for(i = 0; i < num_threads; i++){
        assigns[i] = NULL;
    }

    str = assignment_str;

    while(*str != '\0'){
        if (*str == ',') {
            str++;
        }

        /* parse thread id */
        thread_id = strtod(str, &endptr);
        if (endptr == str) {
            return -1;
        }
        str = endptr;
        if (*str == '-'){       /* if multiple threads are specified like '0-7' */
            str++;
            thread_id_end = strtod(str, &endptr);
            if (endptr == str){
                return -1;
            }
            str = endptr;
        } else {
            thread_id_end = thread_id;
        }

        if (*str++ != ':'){
            return -2;
        }

        /* parse physical core id */
        core_id = strtod(str, &endptr);
        if (endptr == str) {
            return -3;
        }
        str = endptr;

        /* parse memory node id if specified */
        if (*str == ':'){
            str++;
            mem_node_id = strtod(str, &endptr);
            if (str == endptr){
                return -5;
            }
            str = endptr;
        } else {
            mem_node_id = -1;
        }


        if (thread_id >= num_threads || thread_id_end >= num_threads){
            fprintf(stderr, "num_threads: %d, thread_id: %d, thread_id_end: %d\n", num_threads, thread_id, thread_id_end);
            return -4;
        } else {
            for(; thread_id <= thread_id_end; thread_id++){
                if (assigns[thread_id] == NULL){
                    assigns[thread_id] = malloc(sizeof(thread_assignment_t));
                }
                assigns[thread_id]->thread_id = thread_id;

                CPU_ZERO(&assigns[thread_id]->mask);
                CPU_SET(core_id, &assigns[thread_id]->mask);

                assigns[thread_id]->nodemask = 0;

                if (mem_node_id >= 0){
                    assigns[thread_id]->nodemask = 1 << mem_node_id;
                }
            }
        }
    }

    return 0;
}



void
parse_args(int argc, char **argv)
{
    // default values
    option.noop = false;
    option.multi = 1;
    option.assignment_str = NULL;
    option.assigns = NULL;
    option.timeout = 60;
    option.read = true;
    option.write = false;
    option.seq = true;
    option.rand = false;
    option.direct = false;
    option.blk_sz = 64 * KIBI;
    option.ofst_start = 0;
    option.ofst_end = 0;
    option.misalign = 0;
    option.verbose = false;

    // device or file
    if (argc < 14){
        fprintf(stderr, "Device or file is not specified.\n");
        goto error;
    } else {
        option.path = argv[13];
    }

    if (strcmp("true", argv[1]) == 0){
        option.noop = true;
    } else {
        option.noop = false;
    }

    option.multi = strtol(argv[2], NULL, 10);

    // TODO :affinity

    option.timeout = strtol(argv[4], NULL, 10);

    if (strcmp("read", argv[5]) == 0) {
        option.read = true;
        option.write = false;
    } else if (strcmp("write", argv[5]) == 0) {
        option.read = false;
        option.write = true;
    }

    if (strcmp("seq", argv[6]) == 0) {
        option.seq = true;
        option.rand = false;
    } else if (strcmp("rand", argv[6]) == 0) {
        option.seq = false;
        option.rand = true;
    }

    if (strcmp("true", argv[7]) == 0){
        option.direct = true;
    } else {
        option.direct = false;
    }

    option.blk_sz = strtol(argv[8], NULL, 10);

    option.ofst_start = strtol(argv[9], NULL, 10);

    option.ofst_end = strtol(argv[10], NULL, 10);

    option.misalign = strtol(argv[11], NULL, 10);

    if (strcmp("true", argv[12]) == 0){
        option.verbose = true;
    } else {
        option.verbose = false;
    }


    // check device

    if (option.noop == false) {
        if (option.read) {
            if (open(option.path, O_RDONLY) == -1) {
                fprintf(stderr, "Cannot open %s with O_RDONLY\n", option.path);
                goto error;
            }
        } else {
            if (open(option.path, O_WRONLY) == -1) {
                fprintf(stderr, "Cannot open %s with O_WRONLY\n", option.path);
                goto error;
            }
        }
    }

    if (option.assignment_str != NULL){
        int i;
        option.assigns = malloc(sizeof(thread_assignment_t *) * option.multi);
        for (i = 0; i < option.multi; i++){
            option.assigns[i] = NULL;
        }
        int code;
        if ((code = parse_thread_assignment(option.multi,
                                            option.assigns,
                                            option.assignment_str)) != 0){
            fprintf(stderr, "Invalid thread affinity assignment: %s\n",
                       option.assignment_str);
            switch(code){
            case -1:

                break;
            case -2:

                break;
            case -3:

                break;
            case -4:
                fprintf(stderr, "thread id >= # of thread.\n");
                break;
            }
            goto error;
        }
    }

    int64_t path_sz = getsize(option.path);
    if (option.blk_sz * option.ofst_start > path_sz){
        fprintf(stderr, "Too big --offset-start. Maximum: %ld\n",
                   path_sz / option.blk_sz);
        goto error;
    }
    if (option.blk_sz * option.ofst_end > path_sz) {
        fprintf(stderr, "Too big --offset-end. Maximum: %ld\n",
                   path_sz / option.blk_sz);
        goto error;
    }
    if (option.direct && option.blk_sz % 512) {
        fprintf(stderr, "--direct specified. Block size must be multiples of block size of devices.\n");
        goto error;
    }
    if (option.direct && getuid() != 0) {
        fprintf(stderr, "You must be root to use --direct\n");
        goto error;
    }
    if (option.ofst_end == 0) {
        option.ofst_end = path_sz / option.blk_sz;
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
    thread_assignment_t *tass;

    if (option.assigns != NULL){
        tass = option.assigns[th_arg->id];
        if (tass != NULL){
            sched_setaffinity(syscall(SYS_gettid),
                              NPROCESSOR,
                              &tass->mask);
        }
    }

    do_iostress(th_arg);

    return NULL;
}

int
main(int argc, char **argv)
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

    parse_args(argc, argv);

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

    if (option.assigns != NULL){
        for(i = 0; i < option.multi; i++){
            free(option.assigns[i]);
        }
        free(option.assigns);
    }

    return 0;
}


