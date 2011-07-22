
#define _GNU_SOURCE

#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <inttypes.h>

#include <numa.h>
#include <numaif.h>

#include "micbench-utils.h"

#define NPROCESSOR (sysconf(_SC_NPROCESSORS_ONLN))

#define PAGE_SIZE (sysconf(_SC_PAGESIZE))


typedef struct {
    pid_t thread_id;
    cpu_set_t mask;
    unsigned long nodemask;
} thread_assign_spec_t;

struct {
    // multiplicity of memory access
    int multi;

    // memory access mode
    bool seq;
    bool rand;
    bool local;

    // timeout
    int timeout;

    // specifier string of assignments of threads to each (logical) processor
    const char *assign_spec_str;
    thread_assign_spec_t **assign_specs;

    // memory access size
    char *sz_str;
    long size; // guaranteed to be multiple of 1024
    const char *hugetlbfile;
    long hugepage_size;
    char *hugepage_sz_str;

    // cpu usage adjustment
    double cpuusage;

    int num_cswch;

    bool verbose;
} option;

typedef struct perf_counter_rec {
    uintptr_t ops;
    uintptr_t clk;
    double wallclocktime;
} perf_counter_t;

typedef struct {
    int id;
    pthread_t *self;
    thread_assign_spec_t *assign_spec;

    long *working_area; // working area
    long working_size; // size of working area
    int fd;
    perf_counter_t pc;

    pthread_barrier_t *barrier;
} th_arg_t;

static GOptionEntry entries[] =
{
    {"multi", 'm', 0, G_OPTION_ARG_INT, &option.multi,
     "Multiplicity of memory access (default: 1)"},
    {"timeout", 't', 0, G_OPTION_ARG_INT, &option.timeout,
     "Running time of memory access test (in sec) (default: 60sec)"},
    {"rand", 'R', 0, G_OPTION_ARG_NONE, &option.rand,
     "Random memory access (default: sequential access)"},
    {"seq", 'S', 0, G_OPTION_ARG_NONE, &option.seq,
     "Sequential memory access (default)"},
    {"local", 'L', 0, G_OPTION_ARG_NONE, &option.local,
     "Allocate individual memory region for each thread (default: sharing one region)"},
    {"assign", 'a', 0, G_OPTION_ARG_STRING, &option.assign_spec_str,
     "Assign threads to specific cores. Format: <spec> (',' <spec>)*  <spec>:=<thread_id>:<core_id>[:<mem_node_id>]"},
    {"cpuusage", 'u', 0, G_OPTION_ARG_DOUBLE, &option.cpuusage,
     "Specify CPU usage in percent (default: 100)"},
    {"size", 's', 0, G_OPTION_ARG_STRING, &option.sz_str,
     "Size of memory allocation for each thread (default: 1MB)"},
    {"hugetlbfile", 'H', 0, G_OPTION_ARG_STRING, &option.hugetlbfile,
     "Use HugePages if specified. Give a path to hugetlbfs mount point."},
    {"hugepagesize", 'z', 0, G_OPTION_ARG_STRING, &option.hugepage_sz_str,
     "Size of HugePage (default: 2MB)"},
    {"context-switch", 'c', 0, G_OPTION_ARG_INT, &option.num_cswch,
     "Force context switch for each specified iterations. 0 means no forcing. (default: 0)"},
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &option.verbose,
     "Verbose"},
    { NULL }
};

// prototype declarations
uintptr_t read_tsc(void);
void do_memory_stress_seq(perf_counter_t* pc, long *working_area, long working_size, pthread_barrier_t *barrier);
void do_memory_stress_rand(perf_counter_t* pc, long *working_area, long working_size, pthread_barrier_t *barrier);

// generate random number which is more than or equal to 'from' and less than 'to' - 1
// TODO: 48bit
unsigned long
rand_range(unsigned long from, unsigned long to)
{
    register unsigned long x;
    x = lrand48();
    x = x % (to - from) + from;

    return x;
}

void
swap_long(long *ptr1, long *ptr2)
{
    long tmp;
    if (ptr1 != ptr2) {
        tmp = *ptr1;
        *ptr1 = *ptr2;
        *ptr2 = tmp;
    }
}

void
parse_args(int *argc, char ***argv)
{
    GError *error = NULL;
    GOptionContext *context;

    // default values
    option.multi = 1;
    option.timeout = 60;
    option.verbose = false;
    option.rand = false;
    option.seq = false;
    option.local = false;
    option.assign_spec_str = NULL;
    option.sz_str = "1M";
    option.size = MEBI;
    option.hugetlbfile = NULL;
    option.hugepage_size = 2L * 1024L * 1024L;
    option.hugepage_sz_str = NULL;
    option.cpuusage = 100;

    context = g_option_context_new("");
    g_option_context_add_main_entries(context, entries, NULL);

    if (!g_option_context_parse(context, argc, argv, &error)){
        g_print("option parsing failed: %s\n", error->message);
        exit(EXIT_FAILURE);
    }

    // check mode
    if (option.seq && option.rand) {
        g_print("--seq and --rand cannot be specified at a time\n");
        exit(EXIT_FAILURE);
    }
    if (!option.rand){ option.seq = true;}

    option.size = micbench_parse_size(option.sz_str);
    if (option.size == 0){
        g_print("Invalid argument for --size: '%s'\n", option.sz_str);
        goto error;
    }
    if (option.size % KIBI != 0){
        g_print("SIZE must be multiples of 1024.\n");
        goto error;
    }
    if (option.seq == true && option.size % (4 * KIBI) != 0){
        g_print("SIZE must be multiples of 4096 for sequential access mode.\n");
        goto error;
    }
    if (option.size < 1) {
        g_print("Invalid size specifier: %s\n", option.sz_str);
        goto error;
    }

    if (option.hugepage_sz_str != NULL){
        option.hugepage_size = micbench_parse_size(option.hugepage_sz_str);
        if (option.hugepage_size == 0){
            g_print("Invalid argument for --hugepagesize: '%s'\n", option.hugepage_sz_str);
            goto error;
        }
    }

    return;
error:
    g_print(g_option_context_get_help(context, false, NULL));
    g_option_context_free(context);
    exit(EXIT_FAILURE);
}


/*
 * Thread assignment spec
 *
 * <assign_spec> := <thread_spec> (',' <thread_spec>)*
 * <thread_spec> := <thread_id> ':' <physical_core_id> [':' <mem_node_id>]
 * <thread_id> := [1-9][0-9]* | 0
 * <physical_core_id> := [1-9][0-9]* | 0
 * <mem_mode> := [1-9][0-9]* | 0
 *
 */
int
parse_assign_specs(int num_threads, thread_assign_spec_t **specs, const char *spec_str)
{
    int thread_id;
    int core_id;
    int mem_node_id;

    const char *str;
    char *endptr;

    str = spec_str;

    while(*str != '\0'){
        if (*str == ',') {
            str++;
        }

        thread_id = strtod(str, &endptr);
        if (endptr == str) {
            return -1;
        }
        str = endptr;
        if (*str++ != ':'){
            return -2;
        }

        core_id = strtod(str, &endptr);
        if (endptr == str) {
            return -3;
        }
        str = endptr;

        if (thread_id >= num_threads){
            return -4;
        } else {
            if (specs[thread_id] == NULL){
                specs[thread_id] = malloc(sizeof(thread_assign_spec_t));
            }
            specs[thread_id]->thread_id = thread_id;

            CPU_ZERO(&specs[thread_id]->mask);
            CPU_SET(core_id, &specs[thread_id]->mask);

            specs[thread_id]->nodemask = 0;
        }

        if (*str == ':'){
            str++;
            mem_node_id = strtod(str, &endptr);
            if (str == endptr){
                return -5;
            }
            str = endptr;

            specs[thread_id]->nodemask = 1 << mem_node_id;

            if(option.verbose == true)
                g_printerr("assign spec: thread_id=%d, core_id=%d, mem_node_id=%d\n",
                           thread_id, core_id, mem_node_id);
        } else {
            if(option.verbose == true)
                g_printerr("assign spec: thread_id=%d, core_id=%d\n",
                           thread_id, core_id);
        }
    }

    return 0;
}

void *
thread_handler(void *arg)
{
    th_arg_t *th_arg = (th_arg_t *) arg;

    if (th_arg->assign_spec != NULL) {
        g_printerr("Set affinity of thread %d\n", th_arg->id);
        sched_setaffinity(syscall(SYS_gettid),
                          NPROCESSOR,
                          &th_arg->assign_spec->mask);
    }

    if (option.cpuusage > 0) {
        if (option.rand == true){
            do_memory_stress_rand(&th_arg->pc, th_arg->working_area, th_arg->working_size, th_arg->barrier);
        } else {
            do_memory_stress_seq(&th_arg->pc, th_arg->working_area, th_arg->working_size, th_arg->barrier);
        }
    } else {
        sleep(option.timeout);
    }

    pthread_exit(NULL);
}

uintptr_t inline
read_tsc(void)
{
    uintptr_t ret;
    uint32_t eax, edx;
    __asm__ volatile("cpuid; rdtsc;"
                     : "=a" (eax) , "=d" (edx)
                     :
                     : "%ebx", "%ecx");
    ret = ((uint64_t)edx) << 32 | eax;
    return ret;
}

// uintptr_t
// read_tsc(void)
// {
//     uintptr_t ret;
//     struct timespec tp;
//     clock_gettime(CLOCK_MONOTONIC, &tp);
//     ret = tp.tv_sec * 1000000000 + tp.tv_nsec;
//     g_print("%ld\n", ret);
//     return ret;
// }

void
do_memory_stress_seq(perf_counter_t* pc,
                     long *working_area,
                     long working_size,
                     pthread_barrier_t *barrier)
{
    unsigned long iter_count;
    unsigned long i;
    struct timeval start_tv;
    register long *ptr;
    register long *ptr_end;

    register uintptr_t t0, t1;
    register int cswch_counter = 0;

    if (option.cpuusage < 100) {
        iter_count = sizeof(long) * 16 * MEBI / working_size;
    } else {
        iter_count = sizeof(long) * GIBI / working_size;
    }
    if (iter_count == 0) {
        iter_count = 1;
    }

    double t = 0;
    double uf = (100 - option.cpuusage) / option.cpuusage; // cpu usage factor
    int j = 0;
    struct timespec sleeptime;

    pthread_barrier_wait(barrier);
    GETTIMEOFDAY(&start_tv);
    while((t = mv_elapsed_time_from(&start_tv)) < option.timeout){
        // g_print("loop\n");
        t0 = read_tsc();
        for(i = 0;i < iter_count;i++){
            ptr = working_area;
            ptr_end = working_area + (working_size / sizeof(long));
            for(;ptr < ptr_end;){
                // scan & increment 1KB segment
#include "micbench-mem-inner.c"
            }
            if (option.num_cswch > 0 && option.num_cswch == cswch_counter++){
                cswch_counter = 0;
                sched_yield();
            }
        }
        t1 = read_tsc();
        pc->clk += t1 - t0;
        pc->ops += MEM_INNER_LOOP_SEQ_NUM_OPS * (working_size / MEM_INNER_LOOP_SEQ_REGION_SIZE) * iter_count;
        if (option.cpuusage < 100){
            double timeslice = mb_elapsed_time_from(&start_tv) - t - 0.002 * (option.cpuusage / 40) * (option.cpuusage / 40);
            double sleepsec = timeslice * uf;
            sleeptime.tv_sec = floor(sleepsec);
            sleeptime.tv_nsec = (sleepsec - sleeptime.tv_sec) * 1000000000;
            nanosleep(&sleeptime, NULL);
            if (j++ > 1){
                // g_print("sleepsec: %lf\n", sleepsec);
                j = 0;
            }
        }
    }
    if(option.verbose == true) g_print("loop end: t=%lf\n", t);
    pc->wallclocktime = t;
}

void
do_memory_stress_rand(perf_counter_t* pc,
                      long *working_area,
                      long working_size,
                      pthread_barrier_t *barrier)
{
    unsigned long iter_count;
    register unsigned long i;
    struct timeval start_tv;
    register long *ptr;
    long *ptr_start;
    long *ptr_end;

    register uintptr_t t0, t1;
    register int cswch_counter = 0;

    iter_count = KIBI;

    // initialize pointer loop
    srand48(syscall(SYS_gettid) + time(NULL));
    ptr_start = working_area;
    ptr_end = working_area + (working_size / sizeof(long));

    for(ptr = ptr_start;ptr < ptr_end;ptr += 64 / sizeof(long)){ // assume cache line is 64B
        *ptr = (long)(ptr + 64 / sizeof(long));
    }
    *(ptr_end - (64 / sizeof(long))) = (long) ptr_start;

    // shuffle pointer loop
    unsigned long num_cacheline;
    unsigned long ofst;
    num_cacheline = working_size / 64;

    GETTIMEOFDAY(&start_tv);
    for(i = 0; i < num_cacheline; i++){
        long *ptr1, *ptr1_succ, *ptr2, *ptr2_succ;
    retry:
        ofst = g_rand_int_range(rand, 0, num_cacheline - i);
        ptr1 = ptr_start + (i * 64 / sizeof(long));
        ptr1_succ = (long *) *ptr1;
        ptr2 = ptr_start + ((i + ofst) * 64 / sizeof(long));
        ptr2_succ = (long *) *ptr2;

        if (ptr1_succ == ptr2){
            *ptr1 = (long) ptr2_succ;
            *ptr1_succ = *ptr2_succ;
            *ptr2_succ = (long) ptr1_succ;
        } else {
            *ptr1 = (long) ptr2_succ;
            *ptr2 = (long) ptr1_succ;
            swap_long(ptr1_succ, ptr2_succ);
        }

        if (ptr1 == (long*) *ptr1 ||
            ptr2 == (long*) *ptr2 ||
            ptr2_succ == (long*)*ptr2_succ){
            goto retry;
            printf("i=%ld\n"
                   "ptr1:\t%ld\t->\t%ld\n"
                   "ptr2:\t%ld\t->\t%ld\n"
                   "ptr2s:\t%ld\t->\t%ld\n",
                   i,
                   (ptr1 - ptr_start) / 8,
                   ((long*)*ptr1 - ptr_start) / 8,
                   (ptr2 - ptr_start) / 8,
                   ((long*)*ptr2 - ptr_start) / 8,
                   (ptr2_succ - ptr_start) / 8,
                   ((long*)*ptr2_succ - ptr_start) / 8);
            exit(1);
        }
    }
    if(option.verbose == true) {
        g_printerr("shuffle time: %lf\n", mb_elapsed_time_from(&start_tv));
    }

    GETTIMEOFDAY(&start_tv);
    // check loop
    unsigned long counter;
    for(counter = 1, ptr = (long *) *ptr_start;
        ptr != ptr_start;
        ptr = (long *) *ptr){
        // printf("%ld\t->\t%ld\n",
        //        ((long) ptr - (long) ptr_start) / 64,
        //        (*ptr - (long) ptr_start) / 64);
        counter++;
    }
    if (counter != num_cacheline){
        g_printerr("initialization failed. counter=%ld\n", counter);
        exit(EXIT_FAILURE);
    }

    // go through pointer loop for evicting cache lines
    ptr = (long *) *ptr_start;
    while(ptr != ptr_start){
        ptr = (long *) *ptr;
    }

    double t = 0;
    double uf = (100 - option.cpuusage) / option.cpuusage; // cpu usage factor
    int j = 0;
    struct timespec sleeptime;

    pthread_barrier_wait(barrier);
    GETTIMEOFDAY(&start_tv);
    while((t = mb_elapsed_time_from(&start_tv)) < option.timeout){
        t0 = read_tsc();
        ptr = working_area;
        for(i = 0;i < iter_count;i++){
            // read & write cache line 1024 times
#include "micbench-mem-inner-rand.c"
            if (option.num_cswch > 0 && option.num_cswch == ++cswch_counter){
                cswch_counter = 0;
                sched_yield();
            }
        }
        t1 = read_tsc();
        pc->clk += t1 - t0;
        pc->ops += iter_count * MEM_INNER_LOOP_RANDOM_NUM_OPS;
        // g_print("loop clk=%ld, ops=%ld\n", pc->clk, pc->ops);
        if (option.cpuusage < 100){
            double timeslice = mb_elapsed_time_from(&start_tv) - t - 0.002 * (option.cpuusage / 40)*(option.cpuusage / 40);
            double sleepsec = timeslice * uf;
            sleeptime.tv_sec = floor(sleepsec);
            sleeptime.tv_nsec = (sleepsec - sleeptime.tv_sec) * 1000000000;
            nanosleep(&sleeptime, NULL);
            if (j++ > 1){
                // g_print("sleepsec: %lf\n", sleepsec);
                j = 0;
            }
        }
    }
    g_print("loop end: t=%lf\n", t);
    pc->wallclocktime = t;
}

int
main(int argc, char **argv)
{
    th_arg_t          *args;
    long              *working_area;
    int                fd;
    int                i;
    pthread_barrier_t *barrier;

    parse_args(&argc, &argv);

    args = malloc(sizeof(th_arg_t) * option.multi);
    barrier = malloc(sizeof(pthread_barrier_t));
    pthread_barrier_init(barrier, NULL, option.multi);

    for(i = 0;i < option.multi;i++){
        args[i].id = i;
        args[i].self = malloc(sizeof(pthread_t));
        args[i].assign_spec = NULL;
        args[i].pc.ops = 0;
        args[i].pc.clk = 0;
        args[i].barrier = barrier;
    }

    if (option.assign_spec_str != NULL) {
        option.assign_specs = malloc(sizeof(thread_assign_spec_t *) * option.multi);
        for (i = 0;i < option.multi;i++){
            option.assign_specs[i] = NULL;
        }
        int s;
        if ((s = parse_assign_specs(option.multi, option.assign_specs, option.assign_spec_str)) != 0){
            g_print("Invalid assignment specifier(%d): %s\n",
                    s,
                    option.assign_spec_str);
            exit(EXIT_FAILURE);
        }
        for(i = 0;i < option.multi;i++){
            if (option.assign_specs[i] != NULL){
                args[i].assign_spec = option.assign_specs[i];
            }
        }
    }

    size_t mmap_size;
    int mmap_flags;
    size_t align;

    mmap_size = option.size;
    mmap_flags = MAP_PRIVATE;
    fd = -1;

    if (option.hugetlbfile != NULL){
        align = option.hugepage_size;
    } else {
        align = PAGE_SIZE;
        mmap_flags |= MAP_ANONYMOUS;
    }
    if (mmap_size % align != 0){
        mmap_size += (align - mmap_size % align);
    }

    if (option.local == true){
        for(i = 0;i < option.multi;i++){
            if (option.hugetlbfile != NULL) {
                args[i].fd = open(option.hugetlbfile, O_CREAT | O_RDWR, 0755);
                g_printerr("mmap_size: %ld\n", mmap_size);
                if (args[i].fd == -1){
                    perror("Failed to open hugetlbfs.\n");
                    g_print("hugetlbfile: %s\n", option.hugetlbfile);
                    exit(EXIT_FAILURE);
                }
            } else {
                args[i].fd = -1;
            }
            args[i].working_size = option.size;
            args[i].working_area = mmap(NULL,
                                        mmap_size,
                                        PROT_READ|PROT_WRITE,
                                        mmap_flags,
                                        args[i].fd,
                                        0);
            if (args[i].working_area == MAP_FAILED){
                perror("mmap(2) failed");
                exit(EXIT_FAILURE);
            }

            if (args[i].assign_spec != NULL){
                if (mbind(args[i].working_area,
                          mmap_size,
                          MPOL_BIND,
                          &args[i].assign_spec->nodemask,
                          numa_max_node()+2,
                          MPOL_MF_STRICT)
                    != 0){
                    switch(errno){
                    case EFAULT:
                        g_printerr("EFAULT"); break;
                    case EINVAL:
                        g_printerr("EINVAL"); break;
                    case EIO:
                        g_printerr("EIO"); break;
                    case ENOMEM:
                        g_printerr("ENOMEM"); break;
                    case EPERM:
                        g_printerr("EPERM"); break;
                    }
                    perror(": mbind(2) failed");
                    exit(EXIT_FAILURE);
                }
            }
            // initialize memories and force allocation of physical memory
            struct timeval tv;
            GETTIMEOFDAY(&tv);
            memset(args[i].working_area, 1, mmap_size);
            memset(args[i].working_area, 0, mmap_size);
            if (option.verbose == true)
                g_printerr("memset time: %f\n", mb_elapsed_time_from(&tv));

        }
    } else {
        if (option.hugetlbfile != NULL) {
            fd = open(option.hugetlbfile, O_CREAT | O_RDWR, 0755);
            if (fd == -1){
                perror("Failed to open hugetlbfs.\n");
                g_print("hugetlbfile: %s\n", option.hugetlbfile);
                exit(EXIT_FAILURE);
            }
        } else {
            fd = -1;
        }

        working_area = mmap(NULL,
                            mmap_size,
                            PROT_READ|PROT_WRITE,
                            mmap_flags,
                            fd,
                            0);
        if (working_area == MAP_FAILED){
            g_print("Cannot allocate memory\n");
            exit(EXIT_FAILURE);
        }

        /*
          http://www.gossamer-threads.com/lists/linux/kernel/461213

          > And not to make things even more confusing, but the way
          > things are designed now, the value I need to pass to mbind()
          > is numa_max_node()+2.  Very confusing.

          maxnodes of mbind(3) seems to require numa_max_node()+2, not
          numa_max_node()+1
         */
        if (args[0].assign_spec != NULL){
            if (mbind(working_area,
                      mmap_size,
                      MPOL_BIND,
                      &args[0].assign_spec->nodemask,
                      numa_max_node()+2,
                      MPOL_MF_STRICT) != 0){
                switch(errno){
                case EFAULT:
                    g_printerr("EFAULT"); break;
                case EINVAL:
                    g_printerr("EINVAL"); break;
                case EIO:
                    g_printerr("EIO"); break;
                case ENOMEM:
                    g_printerr("ENOMEM"); break;
                case EPERM:
                    g_printerr("EPERM"); break;
                }
                perror(": mbind(2) failed");
                exit(EXIT_FAILURE);
            }
        }

        for(i = 0;i < option.multi;i++){
            args[i].working_size = option.size;
            args[i].working_area = working_area;
        }
    }

    struct timeval start_tv;
    struct timeval end_tv;

    GETTIMEOFDAY(&start_tv);
    for(i = 0;i < option.multi;i++){
        pthread_create(args[i].self, NULL, thread_handler, &args[i]);
    }

    for(i = 0;i < option.multi;i++){
        pthread_join(*args[i].self, NULL);
    }
    GETTIMEOFDAY(&end_tv);
    pthread_barrier_destroy(barrier);

    for(i = 0;i < option.multi;i++){
        g_free(args[i].self);
        if (args[i].fd != -1){
            close(args[i].fd);
        }
        if (option.assign_specs != NULL &&
            option.assign_specs[i] != NULL){
            g_free(option.assign_specs[i]);
        }
    }
    if (fd != -1){
        close(fd);
    }


    int64_t ops = 0;
    int64_t clk = 0;
    double wallclocktime = 0.0;
    for(i = 0;i < option.multi;i++){
        ops += args[i].pc.ops;
        clk += args[i].pc.clk;
        wallclocktime += args[i].pc.wallclocktime;
    }

    wallclocktime /= option.multi;
    double rt = ((double)clk)/ops;
    double tp = ops / wallclocktime;

    // print summary
    g_print("access_pattern\t%s\n"
            "multiplicity\t%d\n"
            "local\t%s\n"
            "assign_spec\t%s\n"
            "context_switch\t%d\n"
            "page_size\t%ld\n"
            "size\t%ld\n"
            "use_hugepages\t%s\n"
           ,
            (option.seq ? "sequential" : "random"),
            option.multi,
            (option.local ? "true" : "false"),
            (option.assign_spec_str == NULL ? "null" : option.assign_spec_str),
            option.num_cswch,
            PAGE_SIZE,
            option.size,
            (option.hugetlbfile == NULL ? "false" : "true")
        );
    if (option.hugetlbfile != NULL) {
        g_print("hugetlbfile\t%s\n"
                "hugepage_size\t%ld\n",
                option.hugetlbfile,
                option.hugepage_size);
    }
    if (option.seq == true) {
        g_print("stride_size\t%d\n",
                MEM_INNER_LOOP_SEQ_STRIDE_SIZE);
    }

    g_print("total_ops\t%ld\n"
            "total_clk\t%ld\n"
            "exec_time\t%lf\n"
            "ops_per_sec\t%le\n"
            "clk_per_op\t%le\n"
            "total_exec_time\t%lf\n",
            ops,
            clk,
            wallclocktime,
            tp,
            rt,
            TV2DOUBLE(end_tv) - TV2DOUBLE(start_tv)
        );
    if (option.seq == true) {
        g_print("GB_per_sec\t%lf\n",
                tp * MEM_INNER_LOOP_SEQ_STRIDE_SIZE / 1024 / 1024 / 1024);
    }


    if (option.local == true){
        for(i = 0;i < option.multi;i++){
            munmap(args[i].working_area, mmap_size);
        }
    } else {
        munmap(args[0].working_area, mmap_size);
    }
    g_free(args);

    return 0;
}
