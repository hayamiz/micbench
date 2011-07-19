
#define _GNU_SOURCE

#include <sched.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <numa.h>
#include <numaif.h>

#include <glib.h>

#define NPROCESSOR (sysconf(_SC_NPROCESSORS_ONLN))

#define PAGE_SIZE (sysconf(_SC_PAGESIZE))

// unit
#define KILO 1000
#define KIBI 1024
#define MEBI (KIBI*KIBI)
#define GIBI (KIBI*MEBI)

typedef struct {
    pid_t thread_id;
    cpu_set_t mask;
    gulong nodemask;
} thread_assign_spec_t;

struct {
    // multiplicity of memory access
    gint multi;

    // memory access mode
    gboolean seq;
    gboolean rand;
    gboolean local;

    // timeout
    gint timeout;

    // specifier string of assignments of threads to each (logical) processor
    const gchar *assign_spec_str;
    thread_assign_spec_t **assign_specs;

    // memory access size
    gchar *sz_str;
    gint64 size; // guaranteed to be multiple of 1024

    // cpu usage adjustment
    gdouble cpuusage;

    gint num_cswch;

    gboolean verbose;
} option;

typedef struct perf_counter_rec {
    guint64 ops;
    guint64 clk;
    gdouble wallclocktime;
} perf_counter_t;

typedef struct {
    gint id;
    pthread_t *self;
    thread_assign_spec_t *assign_spec;

    glong *working_area; // working area
    glong working_size; // size of working area
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
    {"context-switch", 'c', 0, G_OPTION_ARG_INT, &option.num_cswch,
     "Force context switch for each specified iterations. 0 means no forcing. (default: 0)"},
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &option.verbose,
     "Verbose"},
    { NULL }
};

// prototype declarations
guint64 read_tsc(void);
void do_memory_stress_seq(perf_counter_t* pc, glong *working_area, glong working_size, pthread_barrier_t *barrier);
void do_memory_stress_rand(perf_counter_t* pc, glong *working_area, glong working_size, pthread_barrier_t *barrier);

// generate random number which is more than or equal to 'from' and less than 'to' - 1
gulong
rand_range(GRand *rand, gulong from, gulong to)
{
    register gulong x;
    x = g_rand_int(rand);
    x = x << (sizeof(guint32) * 8);
    x = x | g_rand_int(rand);
    x = x % (to - from) + from;

    return x;
}

void
swap_glong(glong *ptr1, glong *ptr2)
{
    glong tmp;
    if (ptr1 != ptr2) {
        tmp = *ptr1;
        *ptr1 = *ptr2;
        *ptr2 = tmp;
    }
}

void
parse_args(gint *argc, gchar ***argv)
{
    GError *error = NULL;
    GOptionContext *context;

    // default values
    option.multi = 1;
    option.timeout = 60;
    option.verbose = FALSE;
    option.rand = FALSE;
    option.seq = FALSE;
    option.local = FALSE;
    option.assign_spec_str = NULL;
    option.sz_str = "1M";
    option.size = MEBI;
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
    if (!option.rand){ option.seq = TRUE;}

    // parse size str
    gint len = strlen(option.sz_str);
    gchar suffix = option.sz_str[len - 1];
    glong size;

    size = g_ascii_strtod(option.sz_str, NULL);
    if (isalpha(suffix)) {
        switch(suffix){
        case 'k': case 'K':
            size *= KIBI;
            break;
        case 'm': case 'M':
            size *= MEBI;
            break;
        case 'g': case 'G':
            size *= GIBI;
            break;
        }
    }
    if (size % KIBI != 0){
        g_print("SIZE must be multiples of 1024.\n");
        goto error;
    }
    if (option.seq == TRUE && size % (4 * KIBI) != 0){
        g_print("SIZE must be multiples of 4096 for sequential access mode.\n");
        goto error;
    }
    if (size < 1) {
        g_print("Invalid size specifier: %s\n", option.sz_str);
        goto error;
    }
    option.size = (gint64) size;

    return;
error:
    g_print(g_option_context_get_help(context, FALSE, NULL));
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
gint
parse_assign_specs(gint num_threads, thread_assign_spec_t **specs, const gchar *spec_str)
{
    gint thread_id;
    gint core_id;
    gint mem_node_id;

    const gchar *str;
    gchar *endptr;

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
                specs[thread_id] = g_malloc(sizeof(thread_assign_spec_t));
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

            if(option.verbose == TRUE)
                g_printerr("assign spec: thread_id=%d, core_id=%d, mem_node_id=%d\n",
                           thread_id, core_id, mem_node_id);
        } else {
            if(option.verbose == TRUE)
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
        if (option.rand == TRUE){
            do_memory_stress_rand(&th_arg->pc, th_arg->working_area, th_arg->working_size, th_arg->barrier);
        } else {
            do_memory_stress_seq(&th_arg->pc, th_arg->working_area, th_arg->working_size, th_arg->barrier);
        }
    } else {
        sleep(option.timeout);
    }

    pthread_exit(NULL);
}

guint64 inline
read_tsc(void)
{
    guint64 ret;
    guint32 eax, edx;
    __asm__ volatile("cpuid; rdtsc;"
                     : "=a" (eax) , "=d" (edx)
                     :
                     : "%ebx", "%ecx");
    ret = ((guint64)edx) << 32 | eax;
    return ret;
}

// guint64
// read_tsc(void)
// {
//     guint64 ret;
//     struct timespec tp;
//     clock_gettime(CLOCK_MONOTONIC, &tp);
//     ret = tp.tv_sec * 1000000000 + tp.tv_nsec;
//     g_print("%ld\n", ret);
//     return ret;
// }

void
do_memory_stress_seq(perf_counter_t* pc,
                     glong *working_area,
                     glong working_size,
                     pthread_barrier_t *barrier)
{
    gulong iter_count;
    gulong i;
    GTimer *timer = g_timer_new();
    register glong *ptr;
    register glong *ptr_end;

    register guint64 t0, t1;
    register gint cswch_counter = 0;

    if (option.cpuusage < 100) {
        iter_count = sizeof(glong) * 16 * MEBI / working_size;
    } else {
        iter_count = sizeof(glong) * GIBI / working_size;
    }
    if (iter_count == 0) {
        iter_count = 1;
    }

    gdouble t = 0;
    gdouble uf = (100 - option.cpuusage) / option.cpuusage; // cpu usage factor
    gint j = 0;
    struct timespec sleeptime;

    pthread_barrier_wait(barrier);
    g_timer_start(timer);
    while((t = g_timer_elapsed(timer, NULL)) < option.timeout){
        // g_print("loop\n");
        t0 = read_tsc();
        for(i = 0;i < iter_count;i++){
            ptr = working_area;
            ptr_end = working_area + (working_size / sizeof(glong));
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
            gdouble timeslice = g_timer_elapsed(timer, NULL) - t - 0.002 * (option.cpuusage / 40)*(option.cpuusage / 40);
            gdouble sleepsec = timeslice * uf;
            sleeptime.tv_sec = floor(sleepsec);
            sleeptime.tv_nsec = (sleepsec - sleeptime.tv_sec) * 1000000000;
            nanosleep(&sleeptime, NULL);
            if (j++ > 1){
                // g_print("sleepsec: %lf\n", sleepsec);
                j = 0;
            }
        }
    }
    if(option.verbose == TRUE) g_print("loop end: t=%lf\n", t);
    pc->wallclocktime = t;

    g_timer_destroy(timer);
}

void
do_memory_stress_rand(perf_counter_t* pc,
                      glong *working_area,
                      glong working_size,
                      pthread_barrier_t *barrier)
{
    gulong iter_count;
    register gulong i;
    GTimer *timer = g_timer_new();
    register glong *ptr;
    glong *ptr_start;
    glong *ptr_end;
    GRand *rand;

    register guint64 t0, t1;
    register gint cswch_counter = 0;

    iter_count = KIBI;

    // initialize pointer loop
    rand = g_rand_new_with_seed(syscall(SYS_gettid) + time(NULL));
    ptr_start = working_area;
    ptr_end = working_area + (working_size / sizeof(glong));

    for(ptr = ptr_start;ptr < ptr_end;ptr += 64 / sizeof(glong)){ // assume cache line is 64B
        *ptr = (glong)(ptr + 64 / sizeof(glong));
    }
    *(ptr_end - (64 / sizeof(glong))) = (glong) ptr_start;

    // shuffle pointer loop
    gulong num_cacheline;
    guint32 ofst;
    num_cacheline = working_size / 64;

    GTimer *tt = g_timer_new(); g_timer_start(tt);
    for(i = 0; i < num_cacheline; i++){
        glong *ptr1, *ptr1_succ, *ptr2, *ptr2_succ;
    retry:
        ofst = g_rand_int_range(rand, 0, num_cacheline - i);
        ptr1 = ptr_start + (i * 64 / sizeof(glong));
        ptr1_succ = (glong *) *ptr1;
        ptr2 = ptr_start + ((i + ofst) * 64 / sizeof(glong));
        ptr2_succ = (glong *) *ptr2;

        if (ptr1_succ == ptr2){
            *ptr1 = (glong) ptr2_succ;
            *ptr1_succ = *ptr2_succ;
            *ptr2_succ = (glong) ptr1_succ;
        } else {
            *ptr1 = (glong) ptr2_succ;
            *ptr2 = (glong) ptr1_succ;
            swap_glong(ptr1_succ, ptr2_succ);
        }

        if (ptr1 == (glong*) *ptr1 ||
            ptr2 == (glong*) *ptr2 ||
            ptr2_succ == (glong*)*ptr2_succ){
            goto retry;
            printf("i=%ld\n"
                   "ptr1:\t%ld\t->\t%ld\n"
                   "ptr2:\t%ld\t->\t%ld\n"
                   "ptr2s:\t%ld\t->\t%ld\n",
                   i,
                   (ptr1 - ptr_start) / 8,
                   ((glong*)*ptr1 - ptr_start) / 8,
                   (ptr2 - ptr_start) / 8,
                   ((glong*)*ptr2 - ptr_start) / 8,
                   (ptr2_succ - ptr_start) / 8,
                   ((glong*)*ptr2_succ - ptr_start) / 8);
            exit(1);
        }
    }
    if(option.verbose == TRUE)
        g_printerr("shuffle time: %f\n", g_timer_elapsed(tt, NULL));
    g_timer_start(tt);

    // check loop
    gulong counter;
    for(counter = 1, ptr = (glong *) *ptr_start;
        ptr != ptr_start;
        ptr = (glong *) *ptr){
        // printf("%ld\t->\t%ld\n",
        //        ((glong) ptr - (glong) ptr_start) / 64,
        //        (*ptr - (glong) ptr_start) / 64);
        counter++;
    }
    if (counter != num_cacheline){
        g_printerr("initialization failed. counter=%ld\n", counter);
        exit(EXIT_FAILURE);
    }
    
    if (0) { // skip validation
        if (option.verbose == TRUE)
            g_printerr("shuffle-validation time: %f\n", g_timer_elapsed(tt, NULL));
        g_timer_destroy(tt);
    }

    // go through pointer loop for evicting cache lines
    ptr = (glong *) *ptr_start;
    while(ptr != ptr_start){
        ptr = (glong *) *ptr;
    }

    gdouble t = 0;
    gdouble uf = (100 - option.cpuusage) / option.cpuusage; // cpu usage factor
    gint j = 0;
    struct timespec sleeptime;

    pthread_barrier_wait(barrier);
    g_timer_start(timer);
    while((t = g_timer_elapsed(timer, NULL)) < option.timeout){
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
            gdouble timeslice = g_timer_elapsed(timer, NULL) - t - 0.002 * (option.cpuusage / 40)*(option.cpuusage / 40);
            gdouble sleepsec = timeslice * uf;
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

    g_timer_destroy(timer);
}

gint
main(gint argc, gchar **argv)
{
    th_arg_t          *args;
    glong             *working_area;
    gint               i;
    pthread_barrier_t *barrier;

    parse_args(&argc, &argv);

    args = g_malloc(sizeof(th_arg_t) * option.multi);
    barrier = g_malloc(sizeof(pthread_barrier_t));
    pthread_barrier_init(barrier, NULL, option.multi);

    for(i = 0;i < option.multi;i++){
        args[i].id = i;
        args[i].self = g_malloc(sizeof(pthread_t));
        args[i].assign_spec = NULL;
        args[i].pc.ops = 0;
        args[i].pc.clk = 0;
        args[i].barrier = barrier;
    }

    if (option.assign_spec_str != NULL) {
        option.assign_specs = g_malloc(sizeof(thread_assign_spec_t *) * option.multi);
        for (i = 0;i < option.multi;i++){
            option.assign_specs[i] = NULL;
        }
        gint s;
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

    size_t mmap_size = option.size;
    size_t align = PAGE_SIZE;
    if (mmap_size % align != 0){
        mmap_size += (align - mmap_size % align);
    }

    if (option.local == TRUE){
        for(i = 0;i < option.multi;i++){
            args[i].working_size = option.size;
            args[i].working_area = mmap(NULL,
                                        mmap_size,
                                        PROT_READ|PROT_WRITE,
                                        MAP_ANONYMOUS|MAP_PRIVATE,
                                        -1,
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
            GTimer *tt = g_timer_new(); g_timer_start(tt);
            memset(args[i].working_area, 1, mmap_size);
            memset(args[i].working_area, 0, mmap_size);
            if (option.verbose == TRUE)
                g_printerr("memset time: %f\n", g_timer_elapsed(tt, NULL));
            g_timer_destroy(tt);

        }
    } else {
        working_area = mmap(NULL,
                            mmap_size,
                            PROT_READ|PROT_WRITE,
                            MAP_PRIVATE|MAP_ANONYMOUS,
                            -1,
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

    GTimer *timer = g_timer_new();
    g_timer_start(timer);
    for(i = 0;i < option.multi;i++){
        pthread_create(args[i].self, NULL, thread_handler, &args[i]);
    }

    for(i = 0;i < option.multi;i++){
        pthread_join(*args[i].self, NULL);
    }
    g_timer_stop(timer);
    pthread_barrier_destroy(barrier);

    for(i = 0;i < option.multi;i++){
        g_free(args[i].self);
        if (option.assign_specs != NULL &&
            option.assign_specs[i] != NULL){
            g_free(option.assign_specs[i]);
        }
    }


    gint64 ops = 0;
    gint64 clk = 0;
    gdouble wallclocktime = 0.0;
    for(i = 0;i < option.multi;i++){
        ops += args[i].pc.ops;
        clk += args[i].pc.clk;
        wallclocktime += args[i].pc.wallclocktime;
    }

    wallclocktime /= option.multi;
    gdouble rt = ((gdouble)clk)/ops;
    gdouble tp = ops / wallclocktime;

    // print summary
    g_print("access_pattern\t%s\n"
            "multiplicity\t%d\n"
            "local\t%s\n"
            "assign_spec\t%s\n"
            "context_switch\t%d\n"
            "page_size\t%ld\n"
            "size\t%ld\n"
           ,
            (option.seq ? "sequential" : "random"),
            option.multi,
            (option.local ? "true" : "false"),
            (option.assign_spec_str == NULL ? "null" : option.assign_spec_str),
            option.num_cswch,
            PAGE_SIZE,
            option.size
        );
    if (option.seq == TRUE) {
        g_print("stride_size\t%d\n",
                tp * MEM_INNER_LOOP_SEQ_STRIDE_SIZE / (2 << 30));
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
            g_timer_elapsed(timer, NULL)
        );
    if (option.seq == TRUE) {
        g_print("GB_per_sec\t%lf\n",
                tp * MEM_INNER_LOOP_SEQ_STRIDE_SIZE / 1024 / 1024 / 1024);
    }


    if (option.local == TRUE){
        for(i = 0;i < option.multi;i++){
            munmap(args[i].working_area, mmap_size);
        }
    } else {
        munmap(args[0].working_area, mmap_size);
    }
    g_free(args);

    return 0;
}
