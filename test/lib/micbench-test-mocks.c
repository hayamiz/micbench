
#include "micbench-test-mocks.h"

static GHashTable *will_call_table = NULL;
static const char *fail_msg = NULL;

void mb_mock_init(void)
{
    if (will_call_table != NULL) {
        g_hash_table_destroy(will_call_table);
    }

    fail_msg = NULL;
    will_call_table = g_hash_table_new(g_str_hash, g_str_equal);
}

static void
mb_mock_finish_iterator(void *_key, void *_value, void *_user_data)
{
    const char *fname = (const char *) _key;
    GList *mock_args_list = (GList *) _value;
    GList *mock_args;
    mb_mock_arg_t *mock_arg;
    GString *msg;

    if (mock_args_list == NULL) {
        return;
    }

    if (fail_msg != NULL) {
        return;
    }

    msg = g_string_new("(");
    mock_args = mock_args_list->data;

    for(; mock_args != NULL; mock_args = mock_args->next) {
        mock_arg = (mb_mock_arg_t *) mock_args->data;
        switch(mock_arg->type){
        case MOCK_ARG_SKIP:
            g_string_append_printf(msg, "(skipped), ");
            break;
        case MOCK_ARG_INT:
            g_string_append_printf(msg, "%d, ", mock_arg->u._int);
            break;
        case MOCK_ARG_PTR:
            g_string_append_printf(msg, "%p, ", mock_arg->u._ptr);
            break;
        default:
            fprintf(stderr,
                    "mb_mock_finish_iterator: unimplemented type %d\n",
                    mock_arg->type);
            exit(EXIT_FAILURE);
        }
    }
    if (msg->len == 1){
        g_string_append_printf(msg, ")");
    } else {
        msg = g_string_truncate(msg, msg->len - 2);
        g_string_append_printf(msg, ")");
    }

    fail_msg = cut_take_printf("Expected function call: %s %s",
                               fname, msg->str);
    gcut_take_string(msg);
}

void mb_mock_finish(void)
{
    const char *__fail_msg;

    if (will_call_table == NULL) {
        return;
    }

    g_hash_table_foreach(will_call_table,
                         mb_mock_finish_iterator,
                         NULL);

    g_hash_table_destroy(will_call_table);
    will_call_table = NULL;
    __fail_msg = fail_msg;
    fail_msg = NULL;

    if (__fail_msg)
        cut_fail("%s", fail_msg);
}

void
mb_mock_assert_will_call(const char *fname, ...)
{
    va_list args;
    GList *mock_args_list;
    GList *mock_args;
    mb_mock_arg_t *mock_arg;
    mb_mock_arg_type_t mock_arg_type;

    cut_assert_not_null(will_call_table);

    mock_args_list = g_hash_table_lookup(will_call_table, fname);
    mock_args = NULL;

    va_start(args, fname);

    for(;;) {
        mock_arg_type = va_arg(args, mb_mock_arg_type_t);
        if (mock_arg_type == 0) {
            break;
        }

        mock_arg = malloc(sizeof(mb_mock_arg_t));
        cut_take_memory(mock_arg);
        mock_arg->type = mock_arg_type;
        switch(mock_arg_type) {
        case MOCK_ARG_SKIP:
            va_arg(args, void *);
            break;
        case MOCK_ARG_INT:
            mock_arg->u._int = va_arg(args, int);
            break;
        case MOCK_ARG_PTR:
            mock_arg->u._ptr = va_arg(args, void *);
            break;
        default:
            fprintf(stderr,
                    "mb_mock_assert_will_call: unimplemented type %d\n",
                    mock_arg_type);
            exit(EXIT_FAILURE);
        }

        mock_args = g_list_append(mock_args, mock_arg);
    }

    mock_args_list = g_list_append(mock_args_list, mock_args);

    g_hash_table_insert(will_call_table, (char *) fname, mock_args_list);

    va_end(args);
}

void
mb_mock_check(const char *fname, ...)
{
    GList *mock_args_list;
    GList *mock_args;
    mb_mock_arg_t *mock_arg;
    va_list args;
    int i;

    int int_arg;
    void *ptr_arg;

    if (will_call_table == NULL)
        return;

    if (NULL == (mock_args_list = g_hash_table_lookup(will_call_table, fname))) {
        return;
    }

    mock_args = (GList *) mock_args_list->data;
    mock_args_list = g_list_remove(mock_args_list, mock_args);
    g_hash_table_insert(will_call_table, (char *) fname, mock_args_list);

    va_start(args, fname);

    for(i = 1; mock_args != NULL; mock_args = mock_args->next, i++) {
        mock_arg = (mb_mock_arg_t *) mock_args->data;
        switch(mock_arg->type) {
        case MOCK_ARG_SKIP: // no check
            va_arg(args, void *);
            break;
        case MOCK_ARG_INT:
            int_arg = va_arg(args, int);
            cut_assert_equal_int(
                mock_arg->u._int, int_arg,
                cut_message("%d-th arg of %s",
                            i, fname));
            break;
        case MOCK_ARG_PTR:
            ptr_arg = va_arg(args, void *);
            cut_assert_equal_pointer(
                mock_arg->u._ptr, ptr_arg,
                cut_message("%d-th arg of %s",
                            i, fname));
            break;
        default:
            fprintf(stderr,
                    "mb_mock_check: unimplemented type %d\n",
                    mock_arg->type);
            exit(EXIT_FAILURE);
        }
    }

    va_end(args);
}
