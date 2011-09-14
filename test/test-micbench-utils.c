
#include "micbench-test.h"

#include <micbench-utils.h>

/* ---- variables ---- */


/* ---- test function prototypes ---- */
void test_getsize(void);
void test_parse_affinity(void);

/* ---- setup/teardown ---- */
void
cut_setup(void)
{
    cut_set_fixture_data_dir(mb_test_get_fixtures_dir(), NULL);
}

/* ---- test function bodies ---- */
void
test_getsize(void)
{
    int64_t size;
    size = mb_getsize(cut_build_fixture_data_path("no-such-file", NULL));
    cut_assert_equal_int(-1, size);

    size = mb_getsize(cut_build_fixture_data_path("1MB.sparse", NULL));
    cut_assert_equal_int_least64(1 << 20, size);

    size = mb_getsize(cut_build_fixture_data_path("100MB.sparse", NULL));
    cut_assert_equal_int_least64((1 << 20) * 100, size);
}

void
test_parse_affinity(void)
{

}
