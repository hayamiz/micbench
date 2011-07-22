
#include "micbench-utils.h"

double
mb_elapsed_time_from(struct timeval *tv)
{
    double ret;
    struct timeval now;
    if (0 != gettimeofday(&now, NULL)){
        perror("gettimeofday(3) failed ");
        fprintf(stderr, " @%s:%d\n", __FILE__, __LINE__);
    }
    return TV2DOUBLE(now) - TV2DOUBLE(tv);
}
