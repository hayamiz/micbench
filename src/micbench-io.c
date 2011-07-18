
#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE

#include <linux/fs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <malloc.h>
#include <sched.h>

#include <glib.h>

#define NPROCESSOR (sysconf(_SC_NPROCESSORS_ONLN))

// unit
#define KILO 1000
#define KIBI 1024
#define MEBI (KIBI*KIBI)
#define GIBI (KIBI*MEBI)

typedef struct {
    pid_t thread_id;
    cpu_set_t mask;
    gulong nodemask;
} thread_assignment_t;

static struct {
    // multiplicity of IO
    gint multi;

    // access mode
    gboolean seq;
    gboolean rand;
    gboolean direct;
    gboolean read;
    gboolean write;

    // thread affinity assignment
    const gchar *assignment_str;
    thread_assignment_t **assigns;

    // timeout
    gint timeout;

    // block size
    gchar *blk_sz_str;
    gint blk_sz;

    // offset
    gint64 ofst_start;
    gint64 ofst_end;

    gint64 misalign;

    // device or file
    const gchar *path;

    gboolean verbose;

    gboolean noop;
} option;

typedef struct {
    // accumulated iowait time
    gdouble iowait_time;

    // io count (in blocks)
    gint64 count;
} meter_t;

typedef struct {
    gdouble exec_time;
    gdouble iowait_time;
    gint64 count;
    gdouble response_time;
    gdouble iops;
    gdouble bandwidth;
} result_t;

static GOptionEntry entries[] =
{
    {"noop", 'n', 0, G_OPTION_ARG_NONE, &option.noop,
     "Dry-run if this option is specified."},
    {"multi", 'm', 0, G_OPTION_ARG_INT, &option.multi,
     "Multiplicity of IO (default: 1)"},
    {"assign", 'A', 0, G_OPTION_ARG_STRING, &option.assignment_str,
    "Thread affinity assignment. <thread id>:<core id>[:<mem node id>][, ...]"},
    {"timeout", 't', 0, G_OPTION_ARG_INT, &option.timeout,
     "Running time of IO stress test (in sec) (default: 60sec)"},
    {"rand", 'r', 0, G_OPTION_ARG_NONE, &option.rand,
     "Random IO access (default: sequential access)"},
    {"direct", 'd', 0, G_OPTION_ARG_NONE, &option.direct,
     "Use O_DIRECT (default: no). If this flag is specified, block size must be multiples of block size of devices."},
    {"write", 'w', 0, G_OPTION_ARG_NONE, &option.write,
     "Write operation (default: read operation)"},
    {"blocksize", 'b', 0, G_OPTION_ARG_STRING, &option.blk_sz_str,
     "Size of block for each IO (in bytes) (default: 64k)"},
    {"offset-start", 's', 0, G_OPTION_ARG_INT64, &option.ofst_start,
     "Offset (in blocks) to start with (default: 0)"},
    {"offset-end", 'e', 0, G_OPTION_ARG_INT64, &option.ofst_end,
     "Offset (in blocks) to end with (default: the size of device)"},
    {"misalign", 'a', 0, G_OPTION_ARG_INT64, &option.misalign,
     "Misalignment from current position (in byte) (default: 0)"},

    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &option.verbose,
     "Verbose"},

    { NULL }
};


typedef struct {
    gint id;
    pthread_t *self;
    meter_t *meter;
    guint32 common_seed;

    gint fd;
} th_arg_t;


gint64
getsize(const gchar *path)
{
    int fd;
    gint64 size;
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
        gint blk_num;
        gint blk_sz;
        guint64 dev_sz;
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
    g_printerr("== configuration summary ==\n\
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
    g_print("== result ==\n\
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
gint
parse_thread_assignment(gint num_threads, thread_assignment_t **assigns, const gchar *assignment_str)
{
    gint thread_id;
    gint thread_id_end;
    gint core_id;
    gint mem_node_id;
    gint i;

    const gchar *str;
    gchar *endptr;

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
            g_printerr("num_threads: %d, thread_id: %d, thread_id_end: %d\n", num_threads, thread_id, thread_id_end);
            return -4;
        } else {
            for(; thread_id <= thread_id_end; thread_id++){
                if (assigns[thread_id] == NULL){
                    assigns[thread_id] = g_malloc(sizeof(thread_assignment_t));
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
parse_args(gint *argc, gchar ***argv)
{
    GError *error = NULL;
    GOptionContext *context;

    // default values
    option.multi = 1;
    
    option.seq = TRUE;
    option.rand = FALSE;
    option.direct = FALSE;
    option.read = TRUE;
    option.write = FALSE;

    option.assignment_str = NULL;
    option.assigns = NULL;
    
    option.timeout = 60;
    
    option.blk_sz_str = "64k";
    option.blk_sz = 64 * KIBI;

    option.ofst_start = 0;
    option.ofst_end = 0;

    option.misalign = 0;
    
    option.verbose = FALSE;
    option.noop = FALSE;

    context = g_option_context_new("device_or_file");
    g_option_context_add_main_entries(context, entries, NULL);

    if (!g_option_context_parse(context, argc, argv, &error)){
        g_printerr("option parsing failed: %s\n", error->message);
        goto error;
    }

    // device or file
    if (*argc < 2){
        g_printerr("Device or file is not specified.\n");
        goto error;
    } else {
        option.path = (*argv)[1];
    }

    // check mode
    if (option.rand) {
        option.seq = FALSE;
    }
    if (option.write) {
        option.read = FALSE;
    }

    // check device

    if (option.noop == FALSE) {
        if (option.read) {
            if (open(option.path, O_RDONLY) == -1) {
                g_printerr("Cannot open %s with O_RDONLY\n", option.path);
                goto error;
            }
        } else {
            if (open(option.path, O_WRONLY) == -1) {
                g_printerr("Cannot open %s with O_WRONLY\n", option.path);
                goto error;
            }
        }
    }

    // parse size str
    gint len = strlen(option.blk_sz_str);
    gchar suffix = option.blk_sz_str[len - 1];
    gdouble size;

    if (option.assignment_str != NULL){
        gint i;
        option.assigns = g_malloc(sizeof(thread_assignment_t *) * option.multi);
        for (i = 0; i < option.multi; i++){
            option.assigns[i] = NULL;
        }
        gint code;
        if ((code = parse_thread_assignment(option.multi,
                                            option.assigns,
                                            option.assignment_str)) != 0){
            g_printerr("Invalid thread affinity assignment: %s\n",
                       option.assignment_str);
            switch(code){
            case -1:

                break;
            case -2:

                break;
            case -3:

                break;
            case -4:
                g_printerr("thread id >= # of thread.\n");
                break;
            }
            goto error;
        }
    }

    size = g_ascii_strtod(option.blk_sz_str, NULL);
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
    if (size < 1) {
        g_printerr("Invalid size specifier: %s\n", option.blk_sz_str);
        goto error;
    }
    option.blk_sz = (gint) size;

    gint64 path_sz = getsize(option.path);
    if (option.blk_sz * option.ofst_start > path_sz){
        g_printerr("Too big --offset-start. Maximum: %ld\n",
                   path_sz / option.blk_sz);
        goto error;
    }
    if (option.blk_sz * option.ofst_end > path_sz) {
        g_printerr("Too big --offset-end. Maximum: %ld\n",
                   path_sz / option.blk_sz);
        goto error;
    }
    if (option.direct && option.blk_sz % 512) {
        g_printerr("--direct specified. Block size must be multiples of block size of devices.\n");
        goto error;
    }
    if (option.ofst_end == 0) {
        option.ofst_end = path_sz / option.blk_sz;
    }

    g_option_context_free(context);
    return;
error:
    g_print(g_option_context_get_help(context, FALSE, NULL));
    g_option_context_free(context);
    exit(EXIT_FAILURE);
}

static inline ssize_t
iostress_readall(gint fd, gchar *buf, size_t size)
{
    size_t sz = size;
    ssize_t ret;

    while(TRUE) {
        if ((ret = read(fd, buf, sz)) == -1){
            g_print("fd=%d, buf=%p, sz=%ld\n", fd, buf, sz);
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
iostress_writeall(gint fd, const gchar *buf, size_t size)
{
    size_t sz = size;
    ssize_t ret;

    while(TRUE) {
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
    GTimer *wallclock = g_timer_new();
    GTimer *timer = g_timer_new();
    meter_t *meter = th_arg->meter;
    GRand *rand = g_rand_new_with_seed(th_arg->common_seed + th_arg->id);
    gint fd = th_arg->fd;
    gint64 ofst = 0;
    gint64 addr;
    void *buf;
    gint i;

    register gdouble iowait_time = 0;
    register gint64 io_count = 0;

    buf = memalign(option.blk_sz, option.blk_sz);

    g_timer_start(wallclock);
    if (option.rand){
        while (g_timer_elapsed(wallclock, NULL) < option.timeout) {
            for(i = 0;i < 100; i++){
                ofst = (gint64) g_random_double_range((gdouble) option.ofst_start,
                                                      (gdouble) option.ofst_end);
                addr = ofst * option.blk_sz + option.misalign;
                if (lseek64(fd, addr, SEEK_SET) == -1){
                    perror("do_iostress:lseek64");
                    exit(EXIT_FAILURE);
                }
                g_timer_start(timer);
                if (option.read) {
                    iostress_readall(fd, buf, option.blk_sz);
                } else if (option.write) {
                    iostress_writeall(fd, buf, option.blk_sz);
                }
                iowait_time += g_timer_elapsed(timer, NULL);
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

        while (g_timer_elapsed(wallclock, NULL) < option.timeout) {
            for(i = 0;i < 100; i++){
                g_timer_start(timer);
                if (option.read) {
                    iostress_readall(fd, buf, option.blk_sz);
                } else if (option.write) {
                    iostress_writeall(fd, buf, option.blk_sz);
                }
                iowait_time += g_timer_elapsed(timer, NULL);
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

    g_timer_destroy(timer);
    g_timer_destroy(wallclock);
    g_rand_free(rand);
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

gint
main(gint argc, gchar **argv)
{
    th_arg_t *th_args;
    gint i;
    gint fd = 0;
    gint flags;
    GTimer *timer;
    result_t result;
    meter_t *meter;
    guint32 common_seed;

    parse_args(&argc, &argv);

    if (option.noop == TRUE){
        print_option();
        exit(EXIT_SUCCESS);
    }
    if (option.verbose){
        print_option();
    }

    th_args = g_malloc(sizeof(th_arg_t) * option.multi);

    timer = g_timer_new();

    if (option.read) {
        flags = O_RDONLY;
    } else {
        flags = O_WRONLY;
    }
    if (option.direct) {
        flags |= O_DIRECT;
    }

    common_seed = (guint32) time(NULL);
    for(i = 0;i < option.multi;i++){
        if ((fd = open(option.path, flags)) == -1){
            perror("main:open(2)");
            exit(EXIT_FAILURE);
        }

        th_args[i].id          = i;
        th_args[i].self        = g_malloc(sizeof(pthread_t));
        th_args[i].fd          = fd;
        th_args[i].common_seed = common_seed;
        meter              = th_args[i].meter = g_malloc(sizeof(meter_t));
        meter->iowait_time = 0;
        meter->count       = 0;
    }
    
    g_timer_start(timer);
    for(i = 0;i < option.multi;i++){
        pthread_create(th_args[i].self, NULL, thread_handler, &th_args[i]);
    }

    for(i = 0;i < option.multi;i++){
        pthread_join(*th_args[i].self, NULL);
    }
    g_timer_stop(timer);
    close(fd);

    gint64 count_sum = 0;
    gdouble iowait_time_sum = 0;
    for(i = 0;i < option.multi;i++){
        meter = th_args[i].meter;
        count_sum += meter->count;
        iowait_time_sum += meter->iowait_time;
    }

    result.exec_time = g_timer_elapsed(timer, NULL);
    result.iowait_time = iowait_time_sum / option.multi;
    result.response_time = iowait_time_sum / count_sum;
    result.iops = count_sum / result.exec_time;
    result.bandwidth = count_sum * option.blk_sz / result.exec_time;

    print_result(&result);

    g_timer_destroy(timer);
    for(i = 0;i < option.multi;i++){
        g_free(th_args[i].meter);
        g_free(th_args[i].self);
    }
    g_free(th_args);

    g_free(option.assignment_str);
    if (option.assigns != NULL){
        for(i = 0; i < option.multi; i++){
            g_free(option.assigns[i]);
        }
        g_free(option.assigns);
    }

    return 0;
}


