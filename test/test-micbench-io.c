
#include "micbench-test.h"

#include <micbench-io.h>

/* ---- variables ---- */
static micbench_io_option_t option;
static gchar *dummy_file;
static char *argv[1024];

/* ---- test function prototypes ---- */
void test_parse_args_noarg(void);
void test_parse_args_defaults(void);
void test_parse_args_rw_modes(void);
void test_parse_args_rwmix_mode(void);
void test_mb_read_or_write(void);

/* ---- cutter setup/teardown ---- */
void
cut_setup(void)
{
    cut_set_fixture_data_dir(mb_test_get_fixtures_dir(), NULL);
    dummy_file = cut_build_fixture_data_path("1MB.sparse", NULL);

    // set dummy execution file name in argv for parse_args
    argv[0] = "./dummy";
}

void
cut_teardown(void)
{

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
test_mb_read_or_write(void)
{
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
    int i;
    double write_ratio = 0.0;
    for(i = 0; i < 100000; i++){
        if (MB_DO_WRITE == mb_read_or_write())
            write_ratio += 1.0;
    }
    write_ratio /= 100000;
    cut_assert_equal_double(0.5, 0.001, write_ratio);
}
