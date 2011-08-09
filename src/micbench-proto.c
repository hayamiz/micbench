#define _GNU_SOURCE

#include "micbench.h"

struct {
    // multiplicity and affinities
    int multi;
    mb_affinity_t **affinities;

    // timeout
    int timeout;

    // count
    long count;

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
    option.count = 2 << 26;
    option.verbose = false;

    optind = 1;
    while ((optchar = getopt(argc, argv, "+m:t:a:c:v")) != -1) {
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
    for(i = 0; i < option.count; i++){
        pthread_spin_lock(arg->slock);
        pthread_spin_unlock(arg->slock);
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

    // do some jobs
    thread_job(th_arg);

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
    printf("multiplicity\t%d\n",
           option.multi
        );
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
