
#include "micbench-test.h"

#include <micbench-btreplay.h>

/* ---- variables ---- */
static mb_btreplay_option_t option;
static char **argv;
static char *btdump_path;
static char *target_device;

/* ---- test function prototypes ---- */
void test_parse_args_without_btdump_path(void);
void test_parse_args_defaults(void);
void test_parse_args_verbose(void);

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
    argv = cut_take_memory(malloc(sizeof(char *) * 1024));
    bzero(argv, sizeof(char *) * 1024);
    argv[0] = "./dummyprogname";
}

void
cut_teardown(void)
{
    
}

/* ---- test function bodies ---- */

void
test_parse_args_without_btdump_path(void)
{
    // cut_assert_not_equal_int(0, mb_btreplay_parse_args(1, argv, &option));
}

void
test_parse_args_defaults(void)
{
//    argv[1] = btdump_path;
//    argv[2] = (char *) target_device;
//    cut_assert_equal_int(0, mb_btreplay_parse_args(3, argv, &option));
//    cut_assert_equal_boolean(false, option.verbose);
//    cut_assert_equal_boolean(false, option.direct);
//    cut_assert_equal_string(btdump_path, option.btdump_path);
//    cut_assert_equal_string(target_device, option.target_path);
}

void
test_parse_args_verbose(void)
{
 //    argv[1] = "-v";
 //    argv[2] = btdump_path;
 //    cut_assert_equal_int(0, mb_btreplay_parse_args(3, argv, &option));
 //    cut_assert_equal_boolean(true, option.verbose);
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
