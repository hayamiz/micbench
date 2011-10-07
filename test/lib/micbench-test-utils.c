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

const void **
mb_take_gpointer_array(const void *ptr, ...)
{
    va_list list;
    int i;
    void **ret;
    void *cur_ptr;

    va_start(list, ptr);
    ret = malloc(sizeof(void *));
    ret[0] = (void *) ptr;
    for(i = 0; ; i++){
        cur_ptr = va_arg(list, void *);
        if (cur_ptr != NULL) {
            ret = realloc(ret, sizeof(const void *) * (i + 2));
            ret[i+1] = cur_ptr;
        } else {
            break;
        }
    }
    va_end(list);
    cut_take_memory(ret);

    return (const void **) ret;
}
