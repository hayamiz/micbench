
#include "micbench-test-mocks.h"

static GList *mock_history = NULL;

/* ---- utility functions ---- */

void
mb_mock_history_set(GList *history)
{
    if (mock_history != NULL) {
        mb_mock_history_destroy();
    }
    mock_history = history;
}

GList *
mb_mock_history_get(void)
{
    return mock_history;
}

void
mb_mock_history_append(mb_mock_hentry_t *entry)
{
    mock_history = g_list_append(mock_history, entry);
}

void
mb_mock_history_destroy(void)
{
    GList *list = mock_history;
    while(list != NULL) {
        mb_mock_hentry_destroy(list->data);
    }
    g_list_free(mock_history);
    mock_history = NULL;
}

mb_mock_hentry_t *
mb_mock_hentry_make(const char *fname, const void **fargs)
{
    mb_mock_hentry_t *entry;
    entry = malloc(sizeof(mb_mock_hentry_t));
    bzero(entry, sizeof(mb_mock_hentry_t));
    entry->fname = fname;
    entry->fargs = fargs;

    return entry;
}

void
mb_mock_hentry_destroy(mb_mock_hentry_t *entry)
{
    free(entry);
}

const void *
mb_mock_arg_int(int val)
{
    int *ret = malloc(sizeof(int));
    *ret = val;
    cut_take_memory(ret);
    return (const void *) ret;
}
