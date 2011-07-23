#ifndef MICBENCH_H
#define MICBENCH_H

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <linux/fs.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>

#include <sched.h>
#include <malloc.h>
#include <pthread.h>

#ifdef HAVE_NUMA_H
#    include <numa.h>
#    include <numaif.h>
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
