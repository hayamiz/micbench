#define _GNU_SOURCE

#include "micbench.h"

typedef enum {
    TEST_SPINLOCK,
    TEST_MUTEX,
} mb_testmode_t;

struct {
    // multiplicity and affinities
    int multi;
    mb_affinity_t **affinities;

    // timeout
    int timeout;

    // count
    long count;
    long critical_job_size;
    long noncritical_job_size;

    // mode
    mb_testmode_t mode;

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

    int             fd;
    perf_counter_t  pc;

    pthread_barrier_t *barrier;

    pthread_mutex_t *mutex;
    pthread_spinlock_t *slock;
} th_arg_t;

// prototype declarations
uintptr_t read_tsc(void);

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
    option.affinities = NULL;
    option.count = 2 << 20;
    option.verbose = false;
    option.mode = TEST_SPINLOCK;
    option.critical_job_size = 10;
    option.noncritical_job_size = 1000;

    optind = 1;
    while ((optchar = getopt(argc, argv, "+m:t:M:C:N:a:c:v")) != -1) {
        switch(optchar){
        case 'm': // multi
            option.multi = strtol(optarg, NULL, 10);
            break;
        case 't': // timeout
            option.timeout = strtol(optarg, NULL, 10);
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
        case 'M': // mode
            if (strcmp("spinlock", optarg) == 0){
                option.mode = TEST_SPINLOCK;
            } else if (strcmp("mutex", optarg) == 0){
                option.mode = TEST_MUTEX;
            } else {
                fprintf(stderr, "No such test mode: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'C': // critical job size
            option.critical_job_size = strtol(optarg, NULL, 10);
            if (option.critical_job_size <= 0){
                fprintf(stderr,
                        "-C requires positive integer but given: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'N': // non critical job size
            option.noncritical_job_size = strtol(optarg, NULL, 10);
            if (option.noncritical_job_size <= 0){
                fprintf(stderr,
                        "-N requires positive integer but given: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'c': // count
            option.count = strtol(optarg, NULL, 10);
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

static void
thread_job(th_arg_t *arg)
{
    long i;
    long j;
    volatile double x;

    x = 0.1;
    asm("# thread job start");
    for(i = 0; i < option.count; i++){
        pthread_spin_lock(arg->slock);
        for(j = 0; j < option.critical_job_size; j++){
            if (x > 1.0)
                x /= 2;
            else
                x += 0.3;
        }
        pthread_spin_unlock(arg->slock);
        for(j = 0; j < option.noncritical_job_size; j++){
            if (x > 1.0)
                x /= 2;
            else
                x += 0.3;
        }
    }
    asm("# thread job end");
}

void *
thread_handler(void *arg)
{
    pid_t tid;
    th_arg_t *th_arg = (th_arg_t *) arg;
    long t0, t1;

    if (th_arg->affinity != NULL){
        tid = syscall(SYS_gettid);
        sched_setaffinity(tid, sizeof(cpu_set_t), &th_arg->affinity->cpumask);
    }

    // do some jobs
    pthread_barrier_wait(th_arg->barrier);
    t0 = mb_read_tsc();
    thread_job(th_arg);
    t1 = mb_read_tsc();

    th_arg->pc.clk = t1 - t0;
    th_arg->pc.ops = option.count;

    pthread_exit(NULL);
}

int
main(int argc, char **argv)
{
    th_arg_t *args;
    int       i;
    pthread_barrier_t *barrier;
    pthread_mutex_t *mutex;
    pthread_spinlock_t *slock;

    if (getenv("MICBENCH") == NULL) {
        fprintf(stderr, "This process may be invoked without micbench command.\n");
    }

    parse_args(argc, argv);

    args = malloc(sizeof(th_arg_t) * option.multi);
    barrier = malloc(sizeof(pthread_barrier_t));
    mutex = malloc(sizeof(pthread_mutex_t));
    slock = malloc(sizeof(pthread_spinlock_t));

    pthread_barrier_init(barrier, NULL, option.multi);
    pthread_mutex_init(mutex, NULL);
    pthread_spin_init(slock, PTHREAD_PROCESS_PRIVATE);

    for(i = 0;i < option.multi;i++){
        args[i].id = i;
        args[i].self = malloc(sizeof(pthread_t));
        args[i].pc.ops = 0;
        args[i].pc.clk = 0;
        args[i].barrier = barrier;
        args[i].mutex = mutex;
        args[i].slock = slock;
        if (option.affinities != NULL) {
            args[i].affinity = option.affinities[i];
        } else {
            args[i].affinity = NULL;
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


    // TODO: print summary and results
    printf("multiplicity\t%d\n"
           "mode\t%s\n"
           "count\t%ld\n"
           "critical_job_size\t%ld\n"
           "noncritical_job_size\t%ld\n",
           option.multi,
           (option.mode == TEST_SPINLOCK ? "spinlock" :
            (option.mode == TEST_MUTEX ? "mutex" : "unknown")),
           option.count,
           option.critical_job_size,
           option.noncritical_job_size
        );
    if (option.affinities != NULL) {
        for(i = 0; i < option.multi; i++){
            char *aff_str;
            if (option.affinities[i] != NULL){
                aff_str = mb_affinity_to_string(option.affinities[i]);
            } else {
                aff_str = NULL;
            }
            printf("affinity_%d\t%s\n",
                   i,
                   (aff_str != NULL ? aff_str : "none"));
            if (aff_str != NULL)
                free(aff_str);
        }
        for(i = 0;i < option.multi;i++){
            mb_free_affinity(option.affinities[i]);
        }
    }

    // print results
    for(i = 0;i < option.multi;i++){
        long clk;
        long ops;
        double clk_per_ops;
        clk = args[i].pc.clk;
        ops = args[i].pc.ops;
        clk_per_ops = (double)clk / (double) ops;
        printf("clk_%d\t%ld\n"
               "ops_%d\t%ld\n"
               "clk_per_ops_%d\t%lf\n",
               i, clk,
               i, ops,
               i, clk_per_ops);
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
