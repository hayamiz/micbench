
#include "micbench-test.h"

#include <micbench-io.h>

/* ---- variables ---- */
static micbench_io_option_t option;
static gchar *dummy_file;
static char *argv[1024];
static mb_aiom_t *aiom;
static mb_res_pool_t *cbpool;
static struct iocb *iocb;
static GList *freed_list;
static GList *will_free_list;

typedef struct {
    void *ptr;
    const char *exp;
    const char *fname;
    int lineno;
} will_free_cell_t;

/* ---- test function prototypes ---- */
void test_parse_args_noarg(void);
void test_parse_args_defaults(void);
void test_parse_args_rw_modes(void);
void test_parse_args_rwmix_mode(void);
void test_parse_args_aio(void);
void test_parse_args_aio_nr_events(void);
void test_mb_read_or_write(void);

void test_mb_aiom_make(void);

void test_mb_aiom_build_blocks(void);
void test_mb_aiom_submit(void);
void test_mb_aiom_prep_pread(void);
void test_mb_aiom_prep_pwrite(void);
void test_mb_aiom_submit_pread(void);
void test_mb_aiom_submit_pwrite(void);
void test_mb_aiom_wait(void);
void test_mb_aiom_waitall(void);
void test_mb_aiom_nr_submittable(void);

void test_mb_res_pool_make(void);
void test_mb_res_pool_destroy(void);
void test_mb_res_pool_push_and_pop(void);

/* ---- utility function prototypes ---- */
static int argc(void);
static void my_free_hook(void *ptr, const void *caller);
#define mb_assert_will_free(__ptr)                                      \
    {                                                                   \
        will_free_cell_t *__cell = malloc(sizeof(will_free_cell_t));    \
        cut_take_memory(__cell);                                        \
        __cell->ptr = __ptr;                                            \
        __cell->exp = #__ptr;                                           \
        __cell->fname = __FILE__;                                       \
        __cell->lineno = __LINE__;                                      \
        will_free_list = g_list_append(will_free_list, __cell);         \
    }

/* mocked function prototypes */
// mocking libaio.h
#include <libaio.h>
static struct iocb **pseudo_io_queue;
static int pseudo_io_queue_len;
int io_setup(int nr_events, io_context_t *ctxp);
int io_submit(io_context_t ctx_id, long nr, struct iocb **iocbpp);
int io_getevents(io_context_t ctx_id, long min_nr, long nr,
                 struct io_event *events, struct timespec *timeout);

/* ---- cutter setup/teardown ---- */
void
cut_setup(void)
{
    cut_set_fixture_data_dir(mb_test_get_fixtures_dir(), NULL);
    dummy_file = (char *) cut_build_fixture_path("1MB.sparse", NULL);

    freed_list = NULL;
    will_free_list = NULL;
    __free_hook = my_free_hook;
    aiom = NULL;
    iocb = NULL;
    cbpool = NULL;
    pseudo_io_queue = NULL;
    pseudo_io_queue_len = 0;

    // set dummy execution file name in argv for parse_args
    bzero(argv, sizeof(argv));
    argv[0] = "./dummy";
}

void
cut_teardown(void)
{
    /* check assertions by mb_assert_will_free */
    GList *candidates;
    for(candidates = will_free_list;
        candidates != NULL;
        candidates = candidates->next) {
        GList *list;
        will_free_cell_t *cell = (will_free_cell_t *) candidates->data;
        bool found = false;
        for(list = freed_list; list != NULL; list = list->next) {
            if (cell->ptr == list->data) {
                found = true;
                break;
            }
        }
        if (found == false) {
            cut_assert_true(found,
                            cut_message("%s (addr %p) should be freed (asserted at %s:%d)",
                                        cell->exp,
                                        cell->ptr,
                                        cell->fname,
                                        cell->lineno));
        }
    }


    __free_hook = NULL;
    if (freed_list != NULL)
        g_list_free(freed_list);
    if (will_free_list != NULL)
        g_list_free(will_free_list);
    if (aiom != NULL)
        mb_aiom_destroy(aiom);
    if (iocb != NULL)
        free(iocb);
    if (cbpool != NULL)
        mb_res_pool_destroy(cbpool);
}

/* ---- utility function bodies ---- */
static int
argc(void)
{
    int ret;
    for(ret = 0; argv[ret] != NULL; ret++){}
    return ret;
}

static void
my_free_hook(void *ptr, const void *caller)
{
    freed_list = g_list_append(freed_list, ptr);
}

/* ---- mocked function bodies ---- */

int
io_setup(int nr_events, io_context_t *ctxp)
{
    mb_mock_check("io_setup", nr_events, ctxp);

    int (*io_setup_org)(int, io_context_t *) =
        (int(*)(int, io_context_t *))dlsym(RTLD_NEXT, "io_setup");

    pseudo_io_queue = malloc(sizeof(struct iocb *) * nr_events);
    cut_take_memory(pseudo_io_queue);
    pseudo_io_queue_len = 0;
    bzero(pseudo_io_queue, sizeof(struct iocb *) * nr_events);

    return io_setup_org(nr_events, ctxp);
}

int
io_submit(io_context_t ctx_id, long nr, struct iocb **iocbpp)
{
    int i;
    mb_mock_check("io_submit", ctx_id, nr, iocbpp);

    for(i = 0; i < nr; i++) {
        pseudo_io_queue[pseudo_io_queue_len++] = iocbpp[i];
    }

    return nr;
}

int io_getevents(io_context_t ctx_id, long min_nr, long nr,
                 struct io_event *events, struct timespec *timeout)
{
    int i;
    int ret;

    mb_mock_check("io_getevents", ctx_id, min_nr, nr, events, timeout);

    ret = 0;
    for(i = 0; i < pseudo_io_queue_len; i++){
        ret++;
        events[i].obj = pseudo_io_queue[i];
    }

    return nr;
}


/* ---- test function bodies ---- */
void
test_parse_args_noarg(void)
{
    cut_assert_not_equal_int(0, parse_args(1, argv, &option));
}

void
test_parse_args_defaults(void)
{
    /* test default values */
    argv[1] = dummy_file;

    cut_assert_equal_int(0, parse_args(2, argv, &option));
    cut_assert_equal_int(1, option.multi);
    cut_assert_false(option.noop);
    cut_assert_null(option.affinities);
    cut_assert_equal_int(60, option.timeout);
    cut_assert_true(option.read);
    cut_assert_false(option.write);
    cut_assert_equal_double(0, 0.001, option.rwmix);
    cut_assert_true(option.seq);
    cut_assert_false(option.rand);
    cut_assert_false(option.direct);
    cut_assert_equal_int(64 * KIBI, option.blk_sz);
    cut_assert_false(option.verbose);
    cut_assert_false(option.aio);
    cut_assert_equal_int(64, option.aio_nr_events);
}

void
test_parse_args_rw_modes(void)
{
    argv[1] = dummy_file;
    cut_assert_equal_int(0, parse_args(2, argv, &option));
    cut_assert_true(option.read);
    cut_assert_false(option.write);

    argv[1] = "-W";
    argv[2] = dummy_file;
    cut_assert_equal_int(0, parse_args(3, argv, &option));
    cut_assert_false(option.read);
    cut_assert_true(option.write);
}

void
test_parse_args_rwmix_mode(void)
{
    argv[1] = "-M";
    argv[2] = "0.5";
    argv[3] = dummy_file;
    cut_assert_equal_int(0, parse_args(4, argv, &option));
    cut_assert_false(option.read);
    cut_assert_false(option.write);
    cut_assert_equal_double(0.5, 0.001, option.rwmix);
}

void
test_parse_args_aio(void)
{
    argv[argc()] = "-A";
    argv[argc()] = dummy_file;
    cut_assert_equal_int(0, parse_args(argc(), argv, &option));
    cut_assert_true(option.aio);
}

void
test_parse_args_aio_nr_events(void)
{
    argv[argc()] = "-E";
    argv[argc()] = "1024";
    argv[argc()] = dummy_file;
    cut_assert_equal_int(0, parse_args(argc(), argv, &option));
    cut_assert_equal_int(1024, option.aio_nr_events);
}

void
test_mb_read_or_write(void)
{
    int i;
    double write_ratio;

    // always do read
    argv[1] = dummy_file;
    parse_args(2, argv, &option);
    mb_set_option(&option);
    cut_assert_equal_int(MB_DO_READ, mb_read_or_write());

    // always do write
    argv[1] = "-W";
    argv[2] = dummy_file;
    parse_args(3, argv, &option);
    mb_set_option(&option);
    cut_assert_equal_int(MB_DO_WRITE, mb_read_or_write());

    argv[1] = "-M";
    argv[2] = "0.5";
    argv[3] = dummy_file;
    parse_args(4, argv, &option);
    mb_set_option(&option);
    write_ratio = 0.0;
    for(i = 0; i < 100000; i++){
        if (MB_DO_WRITE == mb_read_or_write())
            write_ratio += 1.0;
    }
    write_ratio /= 100000;
    cut_assert_equal_double(0.5, 0.001, write_ratio);

    argv[1] = "-M";
    argv[2] = "0.0";
    argv[3] = dummy_file;
    parse_args(4, argv, &option);
    mb_set_option(&option);
    write_ratio = 0.0;
    for(i = 0; i < 100000; i++){
        if (MB_DO_WRITE == mb_read_or_write())
            write_ratio += 1.0;
    }
    write_ratio /= 100000;
    cut_assert_equal_double(0.0, 0.001, write_ratio);

    argv[1] = "-M";
    argv[2] = "1.0";
    argv[3] = dummy_file;
    parse_args(4, argv, &option);
    mb_set_option(&option);
    write_ratio = 0.0;
    for(i = 0; i < 100000; i++){
        if (MB_DO_WRITE == mb_read_or_write())
            write_ratio += 1.0;
    }
    write_ratio /= 100000;
    cut_assert_equal_double(1.0, 0.001, write_ratio);
}

void
test_mb_aiom_make(void)
{
    mb_mock_init();

    mb_mock_assert_will_call("io_setup",
                             MOCK_ARG_INT, 64,
                             MOCK_ARG_SKIP, NULL,
                             NULL);

    aiom = mb_aiom_make(64);
    cut_assert_not_null(aiom);
    cut_assert_equal_int(64, aiom->nr_events);
    cut_assert_equal_int(0, aiom->nr_inflight);
    cut_assert_equal_int(0, aiom->nr_pending);
    cut_assert_not_equal_intptr(0, aiom->context);
    cut_assert_not_null(aiom->cbpool);
    cut_assert_not_null(aiom->pending);
    cut_assert_not_null(aiom->events);
    cut_assert_equal_int(64, mb_aiom_nr_submittable(aiom));

    mb_mock_finish();
}

void
test_mb_aiom_destroy(void)
{
    aiom = mb_aiom_make(64);

    mb_assert_will_free(aiom);
    mb_assert_will_free(aiom->pending);
    mb_assert_will_free(aiom->events);
    mb_assert_will_free(aiom->cbpool);

    mb_aiom_destroy(aiom);
    aiom = NULL;
}

void
test_mb_aiom_build_blocks(void)
{
    cut_pend("TODO");
}

void
test_mb_aiom_submit(void)
{
    mb_mock_init();

    int i;
    int fd = 3;
    char buf[512];

    aiom = mb_aiom_make(64);

    cut_assert_equal_int(0, aiom->nr_pending);
    cut_assert_equal_int(0, aiom->nr_inflight);

    cut_assert_equal_int(0, mb_aiom_submit(aiom));

    for(i = 0; i < 64; i++) {
        mb_aiom_prep_pread(aiom, fd, buf, 512, i * 512);
    }

    cut_assert_equal_int(64, aiom->nr_pending);
    cut_assert_equal_int(0, aiom->nr_inflight);

    mb_mock_assert_will_call("io_submit",
                             MOCK_ARG_PTR, aiom->context,
                             MOCK_ARG_INT, 64,
                             MOCK_ARG_SKIP, NULL,
                             NULL);

    cut_assert_equal_int(64, mb_aiom_submit(aiom));

    cut_assert_equal_int(0, aiom->nr_pending);
    cut_assert_equal_int(64, aiom->nr_inflight);

    mb_mock_finish();
}

void
test_mb_aiom_prep_pread(void)
{
    char buf[512];
    int fd = 3;
    long long offset = 12345;
    struct iocb *_iocb;

    aiom = mb_aiom_make(64);

    cut_assert_equal_int(0, aiom->nr_pending);

    _iocb = mb_aiom_prep_pread(aiom, fd, buf, 512, offset);

    cut_assert_equal_int(1, aiom->nr_pending);

    cut_assert_not_null(_iocb);
    cut_assert_equal_int(63, mb_aiom_nr_submittable(aiom));

    cut_assert_equal_int(IO_CMD_PREAD, _iocb->aio_lio_opcode);
    cut_assert_equal_int(fd, _iocb->aio_fildes);
    cut_assert_equal_pointer(buf, _iocb->u.c.buf);
    cut_assert_equal_int(512, _iocb->u.c.nbytes);
    cut_assert(offset == _iocb->u.c.offset);
}

void
test_mb_aiom_prep_pwrite(void)
{
    char buf[512];
    int fd = 3;
    long long offset = 23456;
    struct iocb *_iocb;

    aiom = mb_aiom_make(64);

    cut_assert_equal_int(0, aiom->nr_pending);

    _iocb = mb_aiom_prep_pwrite(aiom, fd, buf, 512, offset);

    cut_assert_equal_int(1, aiom->nr_pending);

    cut_assert_not_null(_iocb);
    cut_assert_equal_int(63, mb_aiom_nr_submittable(aiom));

    cut_assert_equal_int(IO_CMD_PWRITE, _iocb->aio_lio_opcode);
    cut_assert_equal_int(fd, _iocb->aio_fildes);
    cut_assert_equal_pointer(buf, _iocb->u.c.buf);
    cut_assert_equal_int(512, _iocb->u.c.nbytes);
    cut_assert(offset == _iocb->u.c.offset);
}

void
test_mb_aiom_submit_pread(void)
{
    mb_mock_init();

    char buf[512];
    int fd = 3;

    aiom = mb_aiom_make(64);

    mb_mock_assert_will_call("io_submit",
                             MOCK_ARG_PTR, aiom->context,
                             MOCK_ARG_INT, 1,
                             MOCK_ARG_SKIP, NULL,
                             NULL);
    // pick to-be-used iocb
    struct iocb *head = aiom->cbpool->head->data;

    cut_assert_equal_int(0, aiom->nr_inflight);

    cut_assert_equal_int(1, mb_aiom_submit_pread(aiom, fd, buf, 512, 0));

    cut_assert_equal_int(1, aiom->nr_inflight);

    cut_assert_equal_int(IO_CMD_PREAD, head->aio_lio_opcode);
    cut_assert_equal_int(fd, head->aio_fildes);
    cut_assert_equal_pointer(buf, head->u.c.buf);
    cut_assert_equal_int(512, head->u.c.nbytes);
    cut_assert(0 == head->u.c.offset);

    cut_assert_equal_int(63, mb_aiom_nr_submittable(aiom));

    mb_mock_finish();
}

void
test_mb_aiom_submit_pwrite(void)
{
    mb_mock_init();

    char buf[512];
    int fd = 123;

    aiom = mb_aiom_make(64);

    mb_mock_assert_will_call("io_submit",
                             MOCK_ARG_PTR, aiom->context,
                             MOCK_ARG_INT, 1,
                             MOCK_ARG_SKIP, NULL,
                             NULL);
    struct iocb *head = aiom->cbpool->head->data;

    cut_assert_equal_int(0, aiom->nr_inflight);

    cut_assert_equal_int(1, mb_aiom_submit_pwrite(aiom, fd, buf, 512, 9999));

    cut_assert_equal_int(1, aiom->nr_inflight);

    cut_assert_equal_int(IO_CMD_PWRITE, head->aio_lio_opcode);
    cut_assert_equal_int(fd, head->aio_fildes);
    cut_assert_equal_pointer(buf, head->u.c.buf);
    cut_assert_equal_int(512, head->u.c.nbytes);
    cut_assert(9999 == head->u.c.offset);

    cut_assert_equal_int(63, mb_aiom_nr_submittable(aiom));

    mb_mock_finish();
}

void
test_mb_aiom_wait(void)
{
    mb_mock_init();

    int fd = 3;
    char buf[512];
    long long offset = 3456;

    aiom = mb_aiom_make(64);

    cut_assert_equal_int(0, aiom->nr_pending);
    cut_assert_equal_int(64, mb_aiom_nr_submittable(aiom));
    mb_aiom_prep_pread(aiom, fd, buf, 512, offset);
    cut_assert_equal_int(1, aiom->nr_pending);
    cut_assert_equal_int(63, mb_aiom_nr_submittable(aiom));
    mb_aiom_prep_pread(aiom, fd, buf, 512, offset + 512);
    cut_assert_equal_int(2, aiom->nr_pending);
    cut_assert_equal_int(62, mb_aiom_nr_submittable(aiom));

    mb_mock_assert_will_call("io_submit",
                             MOCK_ARG_PTR, aiom->context,
                             MOCK_ARG_LONG, 2,
                             MOCK_ARG_SKIP, NULL,
                             NULL);
    mb_aiom_submit(aiom);

    cut_assert_equal_int(0, aiom->nr_pending);
    cut_assert_equal_int(2, aiom->nr_inflight);

    mb_mock_assert_will_call("io_getevents",
                             MOCK_ARG_PTR, aiom->context,
                             MOCK_ARG_LONG, 1,
                             MOCK_ARG_LONG, 2,
                             MOCK_ARG_SKIP, NULL,
                             MOCK_ARG_PTR, NULL,
                             NULL);

    cut_assert_equal_int(2, mb_aiom_wait(aiom, NULL));

    cut_assert_equal_int(0, aiom->nr_inflight);
    cut_assert_equal_int(64, mb_aiom_nr_submittable(aiom));

    mb_mock_finish();
}

void
test_mb_aiom_waitall(void)
{
    mb_mock_init();

    int fd = 3;
    char buf[512];
    long long offset = 3456;

    aiom = mb_aiom_make(64);

    mb_aiom_prep_pread(aiom, fd, buf, 512, offset);
    mb_aiom_prep_pread(aiom, fd, buf, 512, offset + 512);

    mb_mock_assert_will_call("io_submit",
                             MOCK_ARG_PTR, aiom->context,
                             MOCK_ARG_LONG, 2,
                             MOCK_ARG_SKIP, NULL,
                             NULL);
    mb_aiom_submit(aiom);

    cut_assert_equal_int(0, aiom->nr_pending);
    cut_assert_equal_int(2, aiom->nr_inflight);

    mb_mock_assert_will_call("io_getevents",
                             MOCK_ARG_PTR, aiom->context,
                             MOCK_ARG_LONG, 2,
                             MOCK_ARG_LONG, 2,
                             MOCK_ARG_SKIP, NULL,
                             MOCK_ARG_PTR, NULL,
                             NULL);

    cut_assert_equal_int(2, mb_aiom_waitall(aiom));

    cut_assert_equal_int(0, aiom->nr_inflight);
    cut_assert_equal_int(64, mb_aiom_nr_submittable(aiom));

    mb_mock_finish();
}

void
test_mb_aiom_nr_submittable(void)
{
    aiom = mb_aiom_make(2);

    cut_assert_equal_int(2, mb_aiom_nr_submittable(aiom));
    mb_aiom_prep_pread(aiom, 0, NULL, 512, 0);
    cut_assert_equal_int(1, mb_aiom_nr_submittable(aiom));
    mb_aiom_prep_pread(aiom, 0, NULL, 512, 0);
    cut_assert_equal_int(0, mb_aiom_nr_submittable(aiom));
}

void
test_mb_res_pool_make(void)
{
    cbpool = mb_res_pool_make(64);
    cut_assert_not_null(cbpool);
    cut_assert_not_null(cbpool->ring);
    cut_assert_not_null(cbpool->head);
    cut_assert_not_null(cbpool->tail);
    cut_assert_equal_int(64, cbpool->size);
    cut_assert_equal_pointer(cbpool->ring, cbpool->head);
    cut_assert_false(cbpool->ring == cbpool->tail);

    // check it is a ring
    int i;
    mb_res_pool_cell_t *cell;
    for(cell = cbpool->ring, i = 0; i < 64; cell = cell->next, i++){}
    cut_assert_equal_pointer(cbpool->ring, cell);

    // initially a pool is empty
    cut_assert_equal_int(0, cbpool->nr_avail);
}

void
test_mb_res_pool_destroy(void)
{
    mb_res_pool_cell_t *cell;
    cbpool = mb_res_pool_make(64);
    cell = cbpool->ring;

    mb_assert_will_free(cbpool);
    mb_assert_will_free(cbpool->ring);
    cell = cbpool->ring->next;
    for(; cell != cbpool->ring; cell = cell->next) {
        mb_assert_will_free(cell);
        mb_assert_will_free(cell->data);
    }

    mb_res_pool_destroy(cbpool);
}

void
test_mb_res_pool_push_and_pop(void)
{
    int i;
    struct iocb *iocb;

    cbpool = mb_res_pool_make(64);

    for(i = 0; i < 64; i++) {
        iocb = malloc(sizeof(struct iocb));
        cut_assert_equal_int(i, cbpool->nr_avail);
        cut_assert_equal_int(0, mb_res_pool_push(cbpool, iocb));
    }

    cut_assert_equal_int(64, cbpool->nr_avail);
    iocb = malloc(sizeof(struct iocb));

    // pushing into full pool fails
    cut_take_memory(iocb);
    cut_assert_equal_int(-1, mb_res_pool_push(cbpool, iocb));

    for(i = 0; i < 64; i++) {
        iocb = mb_res_pool_pop(cbpool);
        cut_assert_not_null(iocb);
        cut_assert_equal_int(64 - i - 1, cbpool->nr_avail,
                             cut_message("poped %d iocbs", i+1));
    }

    // now the pool is empty and pop fails
    iocb = mb_res_pool_pop(cbpool);
    cut_assert_null(iocb);
}
