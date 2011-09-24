
#include "micbench-btreplay.h"

volatile static struct {
    bool stop;
} control;

static mb_btreplay_option_t option;

static void do_thread_job(mb_btreplay_thread_arg_t *arg);

static void *
thread_handler(void *arg)
{
    do_thread_job((mb_btreplay_thread_arg_t *) arg);
    return NULL;
}

static void *
monitor_thread_handler(void *ptr)
{
    int i;
    GAsyncQueue *ioreq_queue;

    ioreq_queue = (GAsyncQueue *) ptr;
    for(;;){
        for(i = 0; i < 10; i++) {
            if (control.stop == true) {
                return NULL;
            }
            sleep(1);
        }
        fprintf(stderr, "[monitor] ioreq queue length: %d\n",
                g_async_queue_length(ptr));
    }

    return NULL;
}

static void
do_thread_job(mb_btreplay_thread_arg_t *arg)
{
    int fd;
    int open_flags;
    mb_btreplay_ioreq_t *ioreq;
    int64_t endpos;
    int64_t ofst;
    size_t sz;
    void *buf;
    size_t bufsz;

    open_flags = O_RDWR;
    if (option.direct){
        open_flags |= O_DIRECT;
    }

    if (-1 == (fd = open(option.target_path, open_flags))){
        perror("Failed open(2)");
        exit(EXIT_FAILURE);
    }

    endpos = -1;
    bufsz = 64 * KIBI;
    buf = memalign(KIBI, bufsz);

    g_async_queue_ref(arg->ioreq_queue);
    for(;;) {
        ioreq = (mb_btreplay_ioreq_t *) g_async_queue_pop(arg->ioreq_queue);
        if (ioreq->stop == true) {
            if (option.verbose)
                fprintf(stderr, "[tid: %d] stopping...\n", arg->tid);
            free(ioreq);
            break;
        }

        int act = ioreq->trace.action & 0xffff;
        bool w = (ioreq->trace.action & BLK_TC_ACT(BLK_TC_WRITE)) != 0;
        if (act == __BLK_TA_ISSUE) {
            ofst = ioreq->trace.sector * 512;
            sz = ioreq->trace.bytes;
            if (sz > bufsz) {
                free(buf);
                bufsz = sz;
                buf = memalign(KIBI, bufsz);
            }
            if (ofst != endpos && false) {
                if (-1 == lseek64(fd, ofst, SEEK_SET)) {
                    fprintf(stderr, "lseek64 failed: errno=%d\n", errno);
                }
                if (option.vverbose)
                    printf("[tid: %d] lseek64 to %ld\n",
                           arg->tid,
                           ofst);
            }
            if (option.vverbose)
                printf("[tid: %d] %s on fd:%d at %ld + %ld\n",
                       arg->tid,
                       (w == true ? "write" : "read"),
                       fd,
                       ofst,
                       sz);
            if (w == false) { // do read
                mb_readall(fd, buf, sz);
            } else { // do write
                mb_writeall(fd, buf, sz);
            }
            endpos = ofst + sz;
        }

        free(ioreq);
    }

    close(fd);
}


int
mb_btreplay_parse_args(int argc, char **argv, mb_btreplay_option_t *option)
{
    char optchar;

    // set default values
    option->verbose = false;
    option->vverbose = false;
    option->btdump_path = NULL;
    option->direct = false;
    option->multi = 1;
    option->timeout = 0;
    option->repeat = false;

    optind = 1;
    while ((optchar = getopt(argc, argv, "+vVm:dt:r")) != -1) {
        switch(optchar) {
        case 'v':
            option->verbose = true;
            break;
        case 'V': // it's too loud except for debugging
            option->verbose = true;
            option->vverbose = true;
            break;
        case 'm':
            option->multi = strtol(optarg, NULL, 10);
            break;
        case 'd':
            option->direct = true;
            break;
        case 't':
            option->timeout = strtol(optarg, NULL, 10);
            break;
        case 'r':
            option->repeat = true;
            break;
        default:
            fprintf(stderr, "Unknown option -%c\n", optchar);
            return -1;
        }
    }

    if (optind == argc) {
        fprintf(stderr, "Input blktrace binary dump is not specified.\n");
    }
    if (optind + 1 == argc) {
        fprintf(stderr, "Target device or file is not specified.\n");
    }
    option->btdump_path = argv[optind++];
    option->target_path = argv[optind++];

    if (option->repeat == true && option->timeout == 0) {
        fprintf(stderr, "Repeat(-r) option must be used with limited timeout (-t) option");
    }

    return 0;
}

int
mb_fetch_blk_io_trace(FILE *file, struct blk_io_trace *trace)
{
    size_t ret;
    char pdu_buf[1024];

    ret = fread(trace, sizeof(struct blk_io_trace), 1, file);
    if (ret != 1) {
        return 1;
    }

    if ((trace->magic & 0xffffff00) != BLK_IO_TRACE_MAGIC) {
        printf("bad magic: %x\n", trace->magic);
        return 1;
    }

    if (trace->pdu_len > 0) {
        fread(pdu_buf, trace->pdu_len, 1, file);
    }

    return 0;
}

void
mb_btreplay_init()
{
    g_thread_init(NULL);
    control.stop = false;
}

int
mb_btreplay_main(int argc, char **argv)
{
    FILE *file;
    pthread_t *threads;
    pthread_t monitor_thread;
    mb_btreplay_thread_arg_t *targs;
    GAsyncQueue *ioreq_queue;
    mb_btreplay_ioreq_t *ioreq;
    int i;

    mb_btreplay_init();

    if (mb_btreplay_parse_args(argc, argv, &option) != 0){
        fprintf(stderr, "Argument error.\n");
        exit(EXIT_FAILURE);
    }

    ioreq_queue = g_async_queue_new();

    threads = malloc(sizeof(pthread_t) * option.multi);
    targs = malloc(sizeof(mb_btreplay_thread_arg_t) * option.multi);

    for(i = 0; i < option.multi; i++) {
        bzero(&targs[i], sizeof(mb_btreplay_thread_arg_t));
        targs[i].tid = i + 1;
        targs[i].ioreq_queue = ioreq_queue;

        if (0 != pthread_create(&threads[i], NULL, thread_handler, &targs[i])) {
            perror("failed to create thread.");
            exit(EXIT_FAILURE);
        }
    }
    if (option.verbose) {
        pthread_create(&monitor_thread, NULL, monitor_thread_handler, ioreq_queue);
    }

    file = fopen(option.btdump_path, "r");

    for(;;) {
        ioreq = malloc(sizeof(mb_btreplay_ioreq_t));
        ioreq->stop = false;
        if (0 != mb_fetch_blk_io_trace(file, &ioreq->trace)){
            free(ioreq);
            break;
        }
        g_async_queue_push(ioreq_queue, ioreq);
    }

    // push ioreq ptrs for telling threads to stop
    for(i = 0; i < option.multi; i++) {
        ioreq = malloc(sizeof(mb_btreplay_ioreq_t));
        ioreq->stop = true;
        g_async_queue_push(ioreq_queue, ioreq);
    }

    fclose(file);

    for(i = 0; i < option.multi; i++) {
        if (0 != pthread_join(threads[i], NULL)) {
            perror("failed to join thread.");
            exit(EXIT_FAILURE);
        }
    }
    control.stop = true;
    if (option.verbose) {
        pthread_join(monitor_thread, NULL);
    }

    free(threads);
    free(targs);
    g_async_queue_unref(ioreq_queue);

    return 0;
}
