#ifndef MICBENCH_H
#define MICBENCH_H

#define _GNU_SOURCE

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <malloc.h>
#include <pthread.h>

#ifdef NUMA_ARCH
#    include <numa.h>
#    include <numaif.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/fs.h>

#ifdef HAVE_IO_URING
#include <liburing.h>
#endif

// unit
#define KILO 1000L
#define KIBI 1024L
#define MEBI (KIBI*KIBI)
#define GIBI (KIBI*MEBI)

#define NPROCESSOR (sysconf(_SC_NPROCESSORS_ONLN))
#define PAGE_SIZE (sysconf(_SC_PAGESIZE))

#include "micbench-utils.h"

#endif
