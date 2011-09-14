
#include "micbench-test.h"

#include <micbench-io.h>

/* ---- variables ---- */
static micbench_io_option_t option;

/* ---- test function prototypes ---- */
void test_parse_args(void);

/* ---- test function bodies ---- */
void test_parse_args_defaults(void)
{
    /* test default values */
    int argc = 1;
    char *argv[2] = {"./dummy", ""};
    // parse_args(argc, (char **) argv, &option);
    // 
    // cut_assert_equal_int(1, option.multi);
}
