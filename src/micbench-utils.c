
#include "micbench-utils.h"

mb_affinity_t *
mb_make_affinity(void)
{
    mb_affinity_t *affinity;

    affinity = malloc(sizeof(mb_affinity_t));
    affinity->tid = -1;
    CPU_ZERO(&affinity->cpumask);
    affinity->nodemask = 0;
    affinity->optarg = NULL;

    return affinity;
}

void
mb_free_affinity(mb_affinity_t *affinity)
{
    if (affinity == NULL) return;
    if (affinity->optarg != NULL)
        free(affinity->optarg);
    free(affinity);
}

mb_affinity_t *
mb_parse_affinity(mb_affinity_t *ret, const char *optarg)
{
    const char *str;
    char *endptr;
    int idx;

    str = optarg;

    if (ret == NULL){
        ret = mb_make_affinity();
    }

    // parse thread id
    ret->tid = strtol(str, &endptr, 10);
    if (endptr == optarg) return NULL;
    if (*endptr != ':') return NULL;

    // parse cpu mask
    CPU_ZERO(&ret->cpumask);
    for(str = endptr + 1, idx = 0; *str != ':'; idx++, str++) {
        if (*str == '\0') return NULL;
        switch(*str){
        case '1':
            CPU_SET(idx, &ret->cpumask);
            break;
        case '0':
            // do nothing
            break;
        default:
            return NULL;
        }
    }

    // parse node mask
    ret->nodemask = 0;
    for(str += 1, idx = 0; *str != '\0'; idx++, str++) {
        switch(*str){
        case '1':
            ret->nodemask |= (1 << idx);
            break;
        case '0':
            // do nothing
            break;
        default:
            return NULL;
        }
    }

    return ret;
}

double
mb_elapsed_time_from(struct timeval *tv)
{
    struct timeval now;
    if (0 != gettimeofday(&now, NULL)){
        perror("gettimeofday(3) failed ");
        fprintf(stderr, " @%s:%d\n", __FILE__, __LINE__);
    }
    return (TV2LONG(now) - TVPTR2LONG(tv)) / 1.0e6;
}

long
mb_elapsed_usec_from(struct timeval *tv)
{
    struct timeval now;
    if (0 != gettimeofday(&now, NULL)){
        perror("gettimeofday(3) failed ");
        fprintf(stderr, " @%s:%d\n", __FILE__, __LINE__);
    }
    return TV2LONG(now) - TVPTR2LONG(tv);
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
