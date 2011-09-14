#include "micbench-test-utils.h"

const gchar *
mb_test_get_test_dir (void)
{
    // TODO:
    const gchar *dir;
    dir = g_getenv("TEST_DIR");
    return dir;
}

const gchar *
mb_test_get_fixtures_dir (void)
{
    // TODO:
    const gchar *dir;
    dir = g_build_filename(mb_test_get_test_dir(), "fixtures", NULL);
    return dir;
}
