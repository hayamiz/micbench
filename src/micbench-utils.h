#ifndef MICBENCH_UTILS_H
#define MICBENCH_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// macros for struct timeval
#define TV2LONG(tv)	(((long) (tv).tv_sec) * 1000000L + (tv).tv_usec)
#define TV2DOUBLE(tv)	(double) (tv).tv_sec + ((double) (tv).tv_usec) / 1000000.0
#define TVPTR2LONG(tv)	(((long) (tv)->tv_sec) * 1000000L + (tv)->tv_usec)
#define TVPTR2DOUBLE(tv)	(double) (tv)->tv_sec + ((double) (tv)->tv_usec) / 1000000.0
#define GETTIMEOFDAY(tv_ptr)                                    \
    {                                                           \
        if (0 != gettimeofday(tv_ptr, NULL)) {                  \
            perror("gettimeofday(3) failed ");                  \
            fprintf(stderr, " @%s:%d\n", __FILE__, __LINE__);   \
        }                                                       \
    }

double mb_elapsed_time_from(struct timeval *tv);

unsigned long mb_rand_range_ulong (unsigned long from, unsigned long to);
long          mb_rand_range_long  (long from, long to);

/*
  <set> := <consecutive_set> | <consecutive_set> '+' <set>
  <consective_set> := <single_set> | <range_set>
  <single_set> := 0 | [1-9] [0-9]*
  <range_set> := <single_set> '-' <single_set>

  ex)
  1 => 0x0000...0001
  3-5 => 0x0...011100
  3-5+7 => 0x0...01011100
 */

#endif
