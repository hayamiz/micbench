#ifndef MICBENCH_UTILS_H
#define MICBENCH_UTILS_H

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#ifndef _LARGEFILE64_SOURCE
#  define _LARGEFILE64_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fs.h>

typedef struct {
    pid_t tid;
    cpu_set_t cpumask;
    unsigned long nodemask;
    char *optarg; // must be free-able pointer on discarding this struct
} mb_affinity_t;


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

mb_affinity_t *mb_make_affinity(void);
void           mb_free_affinity(mb_affinity_t *affinity);

/**
 * mb_parse_affinity:
 * @ret: A pointer to the location where the result will be stored. If NULL, the result is stored in newly malloc-ed location.
 * @optarg: A string passed as an option argument.
 *
 * This function parses an option argument of affinity and returns a
 * pointer to the result. If @ret is not NULL, the result will be
 * stored in @ret and @ret will be returned. If @ret is NULL, the
 * result will be stored in newly malloc-ed location.
 * If some error happen, NULL is returned.
 */
mb_affinity_t *mb_parse_affinity(mb_affinity_t *ret, const char *optarg);

/**
 * mb_affinity_to_string:
 * @affinity: 
 *
 * This function makes human-readable representation of
 * mb_affinity_t. Returned string must be free-ed explicitly.
 * If some error happen, NULL is returned.
 */
char *mb_affinity_to_string(mb_affinity_t *affinity);

double mb_elapsed_time_from(struct timeval *tv);
long   mb_elapsed_usec_from(struct timeval *tv);

unsigned long mb_rand_range_ulong (unsigned long from, unsigned long to);
long          mb_rand_range_long  (long from, long to);

int64_t mb_getsize(const char *path);

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

static inline uint64_t
mb_read_tsc(void)
{
    uintptr_t ret;
    uint32_t eax, edx;
    __asm__ volatile("cpuid; rdtsc;"
                     : "=a" (eax) , "=d" (edx)
                     :
                     : "%ebx", "%ecx");
    ret = ((uint64_t)edx) << 32 | eax;
    return ret;
}

static inline ssize_t
mb_readall(int fd, char *buf, size_t size)
{
    size_t sz = size;
    ssize_t ret;

    for(;;) {
        if ((ret = read(fd, buf, sz)) == -1){
            printf("fd=%d, buf=%p, sz=%lu\n", fd, buf, (unsigned long) sz);
            perror("mb_readall:read");
            exit(EXIT_FAILURE);
        }
    
        if (ret < sz) {
            sz -= ret;
            buf += ret;
        } else {
            break;
        }
    }

    return size;
}

static inline ssize_t
mb_preadall(int fd, char *buf, size_t size, off64_t offset)
{
    ssize_t ret;

    ret = pread64(fd, buf, size, offset);
    if (ret == -1){
        perror("pread64(2) failed");
        fprintf(stderr, "mb_preadall(errno=%d): fd=%d, buf=%p, size=%lu, offset=%lld\n",
                errno, fd, buf, (unsigned long) size, offset);
        exit(EXIT_FAILURE);
    } else if (ret != size) {
        perror("pread64(2) partially failed");
        fprintf(stderr, "mb_preadall: fd=%d, buf=%p, size=%lu, offset=%lld\n", fd, buf, (unsigned long) size, offset);
    }

    return size;
}

static inline ssize_t
mb_writeall(int fd, const char *buf, size_t size)
{
    size_t sz = size;
    ssize_t ret;

    for(;;) {
        if ((ret = write(fd, buf, sz)) == -1){
            perror("mb_writeall:write");
            exit(EXIT_FAILURE);
        }
    
        if (ret < sz) {
            sz -= ret;
            buf += ret;
        } else {
            break;
        }
    }

    return size;
}

static inline ssize_t
mb_pwriteall(int fd, char *buf, size_t size, off64_t offset)
{
    ssize_t ret;

    ret = pwrite64(fd, buf, size, offset);
    if (ret == -1){
        perror("pwrite64(2) failed");
        fprintf(stderr, "mb_pwriteall: fd=%d, buf=%p, size=%lu, offset=%lld\n", fd, buf, (unsigned long) size, offset);
        exit(EXIT_FAILURE);
    } else if (ret != size) {
        perror("pwrite64(2) partially failed");
        fprintf(stderr, "mb_pwriteall: fd=%d, buf=%p, size=%lu, offset=%lld\n", fd, buf, (unsigned long) size, offset);
    }

    return size;
}

#endif
