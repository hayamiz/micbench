
#include "micbench-utils.h"


/* return 0 on failure */
glong
micbench_parse_size(const gchar *sz_str)
{
    gint len;
    gchar suffix;
    gint64 size;

    len = strlen(sz_str);
    suffix = sz_str[len - 1];
    size = g_ascii_strtoll(sz_str, NULL, 10);

    if (size == 0) {
        return 0;
    }

    if (isalpha(suffix)) {
        switch(suffix){
        case 'k': case 'K':
            size *= KIBI;
            break;
        case 'm': case 'M':
            size *= MEBI;
            break;
        case 'g': case 'G':
            size *= GIBI;
            break;
        }
    }

    return size;
}
