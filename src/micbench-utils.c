
#include "micbench-utils.h"


double
mb_elapsed_time_from(struct timeval *tv)
{
    struct timeval now;
    if (0 != gettimeofday(&now, NULL)){
        perror("gettimeofday(3) failed ");
        fprintf(stderr, " @%s:%d\n", __FILE__, __LINE__);
    }
    return TV2DOUBLE(now) - TVPTR2DOUBLE(tv);
}

unsigned long
mb_rand_range_ulong(unsigned long from, unsigned long to)
{
    return ((unsigned long) ((to - from) * drand48())) + from;
}

long
mb_rand_range_long(long from, long to)
{
    return ((long) ((to - from) * drand48())) + from;
}
