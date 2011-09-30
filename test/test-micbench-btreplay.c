
#include "micbench-test.h"

#include <micbench-btreplay.h>

/* ---- variables ---- */
static mb_btreplay_option_t option;
static char **argv;
static char *btdump_path;
static char *target_device;

/* ---- utility function prototypes ---- */
int argc(void);

/* ---- test function prototypes ---- */
void test_parse_args_without_btdump_path(void);
void test_parse_args_defaults(void);
void test_parse_args_verbose(void);
void test_parse_args_very_verbose(void);
void test_parse_args_multi(void);
void test_parse_args_direct(void);
void test_parse_args_timeout(void);
void test_parse_args_repeat(void);

void test_fetch_blk_io_trace(void);

/* ---- cutter setup/teardown ---- */
void
cut_startup(void)
{
    cut_set_fixture_data_dir(mb_test_get_fixtures_dir(), NULL);
    btdump_path = (char *) cut_build_fixture_path("btdumpfile", NULL);
    target_device = "/dev/dummy";
}

void
cut_setup(void)
{
    argv = (char **) cut_take_memory(malloc(sizeof(char *) * 1024));
    bzero(argv, sizeof(char *) * 1024);
    bzero(&option, sizeof(option));
    argv[0] = "./dummyprogname";
}

void
cut_teardown(void)
{
    
}

/* ---- utility function bodies ---- */

int argc(void)
{
    int ret;
    for(ret = 0; argv[ret] != NULL; ret++){}
    return ret;
}

/* ---- test function bodies ---- */

void
test_parse_args_without_btdump_path(void)
{
    cut_assert_not_equal_int(0, mb_btreplay_parse_args(argc(), argv, &option));
}

void
test_parse_args_defaults(void)
{
    argv[argc()] = btdump_path;
    argv[argc()] = (char *) target_device;
    cut_assert_equal_int(0, mb_btreplay_parse_args(argc(), argv, &option));
    cut_assert_false(option.verbose);
    cut_assert_false(option.vverbose);
    cut_assert_false(option.direct);
    cut_assert_false(option.repeat);
    cut_assert_equal_string(btdump_path, option.btdump_path);
    cut_assert_equal_string(target_device, option.target_path);
}

void
test_parse_args_verbose(void)
{
    argv[1] = "-v";
    argv[argc()] = btdump_path;
    argv[argc()] = (char *) target_device;
    cut_assert_equal_int(0, mb_btreplay_parse_args(argc(), argv, &option));
    cut_assert_equal_boolean(true, option.verbose);
}

void
test_parse_args_very_verbose(void)
{
    argv[1] = "-V";
    argv[argc()] = btdump_path;
    argv[argc()] = (char *) target_device;
    cut_assert_equal_int(0, mb_btreplay_parse_args(argc(), argv, &option));
    cut_assert_equal_boolean(true, option.verbose);
    cut_assert_equal_boolean(true, option.vverbose);
}

void
test_parse_args_multi(void)
{
    argv[1] = "-m";
    argv[2] = "4";
    argv[argc()] = btdump_path;
    argv[argc()] = (char *) target_device;
    cut_assert_equal_int(0, mb_btreplay_parse_args(argc(), argv, &option));
    cut_assert_equal_int(4, option.multi);
}

void
test_parse_args_direct(void)
{
    argv[1] = "-d";
    argv[argc()] = btdump_path;
    argv[argc()] = (char *) target_device;
    cut_assert_equal_int(0, mb_btreplay_parse_args(argc(), argv, &option));
    cut_assert_true(option.direct);
}

void
test_parse_args_timeout(void)
{
    argv[1] = "-t";
    argv[2] = "123";
    argv[argc()] = btdump_path;
    argv[argc()] = (char *) target_device;
    cut_assert_equal_int(0, mb_btreplay_parse_args(argc(), argv, &option));
    cut_assert_equal_int(123, option.timeout);
}

void test_parse_args_repeat(void)
{
    argv[1] = "-r";
    argv[2] = "-t";
    argv[3] = "123";
    argv[argc()] = btdump_path;
    argv[argc()] = (char *) target_device;
    cut_assert_equal_int(0, mb_btreplay_parse_args(argc(), argv, &option));
    cut_assert_true(option.repeat);
    cut_assert_equal_int(123, option.timeout);
}

void
test_fetch_blk_io_trace(void)
{
    FILE *dumpfile;
    struct blk_io_trace trace[10];
    int i;

    dumpfile = fopen(btdump_path, "r");
    for (i = 0; i < 10; i++) {
        cut_assert_equal_int(0, mb_fetch_blk_io_trace(dumpfile, &trace[i]));
    }
}
