#define _GNU_SOURCE

#include "micbench.h"

struct {
    // multiplicity of memory access
    int multi;
    mb_affinity_t **affinities;

    // memory access mode
    bool seq;
    bool rand;
    bool local;

    // timeout
    int timeout;

    // memory access size
    long size; // guaranteed to be multiple of 1024
    const char *hugetlbfile;
    long hugepage_size;

    bool verbose;
} option;

typedef struct perf_counter_rec {
    unsigned long ops;
    unsigned long clk;
    double wallclocktime;
} perf_counter_t;

typedef struct {
    int            id;
    pthread_t     *self;
    mb_affinity_t *affinity;

    long           *working_area; // working area
    long            working_size; // size of working area
    int             fd;
    perf_counter_t  pc;

    pthread_barrier_t *barrier;
} th_arg_t;

// prototype declarations
uintptr_t read_tsc(void);
void do_memory_stress_seq(perf_counter_t* pc, long *working_area, long working_size, pthread_barrier_t *barrier);
void do_memory_stress_rand(perf_counter_t* pc, long *working_area, long working_size, pthread_barrier_t *barrier);

// generate random number which is more than or equal to 'from' and less than 'to' - 1
// TODO: 48bit

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
parse_args(int argc, char **argv)
{
    char optchar;
    int idx;

    option.multi = 1;
    option.timeout = 10;
    option.seq = true;
    option.rand = false;
    option.local = false;
    option.affinities = NULL;
    option.size = 1 << 20; // 1MB
    option.hugetlbfile = NULL;
    option.hugepage_size = 1 << 21; // 2MB
    option.verbose = false;

    optind = 1;
    while ((optchar = getopt(argc, argv, "+m:t:SRLa:s:H:z:v")) != -1) {
        switch(optchar){
        case 'm': // multi
            option.multi = strtol(optarg, NULL, 10);
            break;
        case 't': // timeout
            option.timeout = strtol(optarg, NULL, 10);
            break;
        case 'S': // seq.
            option.seq = true;
            option.rand = false;
            break;
        case 'R': // rand.
            option.seq = false;
            option.rand = true;
            break;
        case 'L': // local
            option.local = true;
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
            if (option.affinities == NULL){
                option.affinities = malloc(sizeof(mb_affinity_t *) * option.multi);
                bzero(option.affinities, sizeof(mb_affinity_t *) * option.multi);
            }

            if ((aff = mb_parse_affinity(NULL, optarg)) == NULL){
                fprintf(stderr, "Invalid argument for -a: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            aff->optarg = strdup(optarg);
            option.affinities[aff->tid] = aff;
        }
            break;
        case 's': // size
            option.size = strtol(optarg, NULL, 10);
            break;
        case 'H': // hugetlbfile
            option.hugetlbfile = strdup(optarg);
            break;
        case 'z': // hugepagesize
            option.hugepage_size = strtol(optarg, NULL, 10);
            break;
        case 'v': // verbose
            option.verbose = true;
            break;
        default:
            fprintf(stderr, "Unknown option '-%c'\n", optchar);
            exit(EXIT_FAILURE);
        }
    }
}

void *
thread_handler(void *arg)
{
    pid_t tid;
    th_arg_t *th_arg = (th_arg_t *) arg;

    if (th_arg->affinity != NULL){
        tid = syscall(SYS_gettid);
        sched_setaffinity(tid, sizeof(cpu_set_t), &th_arg->affinity->cpumask);
    }

    if (option.rand == true){
        do_memory_stress_rand(&th_arg->pc,
                              th_arg->working_area,
                              th_arg->working_size,
                              th_arg->barrier);
    } else {
        do_memory_stress_seq(&th_arg->pc,
                             th_arg->working_area,
                             th_arg->working_size,
                             th_arg->barrier);
    }

    pthread_exit(NULL);
}

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

    iter_count = sizeof(long) * GIBI / working_size;
    if (iter_count == 0) {
        iter_count = 1;
    }

    double t = 0;
    register long timeout = option.timeout * 1000000L;

    pthread_barrier_wait(barrier);
    GETTIMEOFDAY(&start_tv);
    if (option.size < 1024) {
        while((t = mb_elapsed_usec_from(&start_tv)) < timeout){
            t0 = mb_read_tsc();
            for(i = 0;i < iter_count;i++){
                ptr = working_area;
                ptr_end = working_area + (working_size / sizeof(long));
                for(;ptr < ptr_end;){
                    // scan & increment consecutive segment
#include "micbench-mem-inner-64.c"
                }
            }
            t1 = mb_read_tsc();
            pc->clk += t1 - t0;
            pc->ops += MEM_INNER_LOOP_SEQ_64_NUM_OPS * (working_size / MEM_INNER_LOOP_SEQ_64_REGION_SIZE) * iter_count;
        }
    } else {
        while((t = mb_elapsed_usec_from(&start_tv)) < timeout){
            t0 = mb_read_tsc();
            for(i = 0;i < iter_count;i++){
                ptr = working_area;
                ptr_end = working_area + (working_size / sizeof(long));
                for(;ptr < ptr_end;){
                    // scan & increment consecutive segment
#include "micbench-mem-inner.c"
                }
            }
            t1 = mb_read_tsc();
            pc->clk += t1 - t0;
            pc->ops += MEM_INNER_LOOP_SEQ_NUM_OPS * (working_size / MEM_INNER_LOOP_SEQ_REGION_SIZE) * iter_count;
        }
    }
    if(option.verbose == true) fprintf(stderr, "loop end: t=%lf\n", t);
    pc->wallclocktime = t / 1e6;
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
        ofst = mb_rand_range_ulong(0, num_cacheline - i);
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
        }
    }
    if(option.verbose == true) {
        fprintf(stderr, "shuffle time: %lf\n", mb_elapsed_time_from(&start_tv));
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
        fprintf(stderr, "initialization failed. counter=%ld\n", counter);
        exit(EXIT_FAILURE);
    }

    // go through pointer loop for evicting cache lines
    ptr = (long *) *ptr_start;
    while(ptr != ptr_start){
        ptr = (long *) *ptr;
    }

    double t = 0;

    pthread_barrier_wait(barrier);
    GETTIMEOFDAY(&start_tv);
    while((t = mb_elapsed_time_from(&start_tv)) < option.timeout){
        t0 = mb_read_tsc();
        ptr = working_area;
        for(i = 0;i < iter_count;i++){
            // read & write cache line 1024 times
#include "micbench-mem-inner-rand.c"
        }
        t1 = mb_read_tsc();
        pc->clk += t1 - t0;
        pc->ops += iter_count * MEM_INNER_LOOP_RANDOM_NUM_OPS;
    }
    printf("loop end: t=%lf\n", t);
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

    if (getenv("MICBENCH") == NULL) {
        fprintf(stderr, "Variable MICBENCH is not set.\n"
                "This process should be invoked via \"micbench\" command.\n");
        exit(EXIT_FAILURE);
    }

    parse_args(argc, argv);

    args = malloc(sizeof(th_arg_t) * option.multi);
    barrier = malloc(sizeof(pthread_barrier_t));
    pthread_barrier_init(barrier, NULL, option.multi);

    for(i = 0;i < option.multi;i++){
        args[i].id = i;
        args[i].self = malloc(sizeof(pthread_t));
        args[i].pc.ops = 0;
        args[i].pc.clk = 0;
        args[i].barrier = barrier;
        if (option.affinities != NULL) {
            args[i].affinity = option.affinities[i];
        } else {
            args[i].affinity = NULL;
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
                if (args[i].fd == -1){
                    perror("Failed to open hugetlbfs.\n");
                    printf("hugetlbfile: %s\n", option.hugetlbfile);
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

            if (args[i].affinity != NULL && args[i].affinity->nodemask != 0){
#ifdef NUMA_ARCH
                if (mbind(args[i].working_area,
                          mmap_size,
                          MPOL_BIND,
                          &args[i].affinity->nodemask,
                          numa_max_node()+2,
                          MPOL_MF_STRICT)
                    != 0){
                    switch(errno){
                    case EFAULT:
                        fprintf(stderr, "EFAULT"); break;
                    case EINVAL:
                        fprintf(stderr, "EINVAL"); break;
                    case EIO:
                        fprintf(stderr, "EIO"); break;
                    case ENOMEM:
                        fprintf(stderr, "ENOMEM"); break;
                    case EPERM:
                        fprintf(stderr, "EPERM"); break;
                    }
                    perror(": mbind(2) failed");
                    exit(EXIT_FAILURE);
                }
#else
                fprintf(stderr, "NUMA specific operation specified, but not operated");
#endif
            }
            // initialize memories and force allocation of physical memory
            struct timeval tv;
            GETTIMEOFDAY(&tv);
            memset(args[i].working_area, 1, mmap_size);
            memset(args[i].working_area, 0, mmap_size);
            if (option.verbose == true)
                fprintf(stderr, "memset time: %f\n", mb_elapsed_time_from(&tv));

        }
    } else {
        if (option.hugetlbfile != NULL) {
            fd = open(option.hugetlbfile, O_CREAT | O_RDWR, 0755);
            if (fd == -1){
                perror("Failed to open hugetlbfs.\n");
                printf("hugetlbfile: %s\n", option.hugetlbfile);
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
            printf("Cannot allocate memory\n");
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
        if (args[0].affinity != NULL  && args[0].affinity->nodemask != 0){
#ifdef NUMA_ARCH
            if (mbind(working_area,
                      mmap_size,
                      MPOL_BIND,
                      &args[0].affinity->nodemask,
                      numa_max_node()+2,
                      MPOL_MF_STRICT) != 0){
                switch(errno){
                case EFAULT:
                    fprintf(stderr, "EFAULT"); break;
                case EINVAL:
                    fprintf(stderr, "EINVAL"); break;
                case EIO:
                    fprintf(stderr, "EIO"); break;
                case ENOMEM:
                    fprintf(stderr, "ENOMEM"); break;
                case EPERM:
                    fprintf(stderr, "EPERM"); break;
                }
                perror(": mbind(2) failed");
                exit(EXIT_FAILURE);
            }
#else
            fprintf(stderr, "NUMA specific operation specified, but not operated");
#endif
        }

        for(i = 0;i < option.multi;i++){
            args[i].working_size = option.size;
            args[i].working_area = working_area;
            args[i].fd = -1;
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
    free(barrier);

    if (fd != -1){
        close(fd);
    }

    unsigned long ops = 0;
    unsigned long clk = 0;
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
    printf("access_pattern\t%s\n"
           "multiplicity\t%d\n"
           "local\t%s\n"
           "page_size\t%ld\n"
           "size\t%ld\n"
           "use_hugepages\t%s\n"
           ,
           (option.seq ? "sequential" : "random"),
           option.multi,
           (option.local ? "true" : "false"),
           PAGE_SIZE,
           option.size,
           (option.hugetlbfile == NULL ? "false" : "true")
        );
    if (option.hugetlbfile != NULL) {
        printf("hugetlbfile\t%s\n"
               "hugepage_size\t%ld\n",
               option.hugetlbfile,
               option.hugepage_size);
    }
    if (option.seq == true) {
        printf("stride_size\t%d\n",
               MEM_INNER_LOOP_SEQ_STRIDE_SIZE);
    }

    printf("total_ops\t%ld\n"
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
        printf("GB_per_sec\t%lf\n",
               tp * MEM_INNER_LOOP_SEQ_STRIDE_SIZE / 1024 / 1024 / 1024);
    }


    if (option.local == true){
        for(i = 0;i < option.multi;i++){
            munmap(args[i].working_area, mmap_size);
        }
    } else {
        munmap(args[0].working_area, mmap_size);
    }

    if (option.affinities != NULL){
        for(i = 0;i < option.multi;i++){
            mb_free_affinity(option.affinities[i]);
        }
    }

    for(i = 0;i < option.multi;i++){
        free(args[i].self);
        if (args[i].fd != -1){
            close(args[i].fd);
        }
    }

    free(args);

    return 0;
}
