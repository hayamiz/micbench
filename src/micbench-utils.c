
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

static int
__mb_bits_to_string(char *bits, int nbits, char *ret)
{
    int i;
    int start_idx, end_idx;
    char buf[1024];
    start_idx = end_idx = -1;
    bzero(buf, sizeof(buf));

    for (i = 0; i < nbits; i++){
        int bits_idx, bits_ofst;
        bits_idx = i / (sizeof(char) * 8);
        bits_ofst = i - (sizeof(char) * 8) * bits_idx;

        if (start_idx < 0) {
            if ((bits[bits_idx] & (1 << bits_ofst)) > 0) {
                start_idx = i;
            }
        } else {
            if ((bits[bits_idx] & (1 << bits_ofst)) > 0) {
                // bit set, continue scanning...
            } else {
                end_idx = i - 1;
                if (strlen(ret) > 0)
                    strcat(ret, ",");

                if (start_idx == end_idx) {
                    sprintf(buf, "%d", start_idx);
                } else {
                    sprintf(buf, "%d-%d", start_idx, end_idx);
                }
                strcat(ret, buf);
                start_idx = -1;
            }
        }
    }

    return 0;
}

char *
mb_affinity_to_string(mb_affinity_t *affinity)
{
    char *ret;
    char buf[1024];
    int retsize;

    if (affinity == NULL){
        return NULL;
    }

    ret = NULL;
    retsize = 0;

    sprintf(buf, "%d:", affinity->tid);
    retsize = strlen(buf) + 1;
    ret = realloc(ret, sizeof(char) * retsize);
    ret[0] = '\0';
    strcat(ret, buf);
    bzero(buf, sizeof(buf));

    __mb_bits_to_string((char *) affinity->cpumask.__bits,
                        sizeof(affinity->cpumask.__bits) * 8,
                        buf);
    retsize += strlen(buf);
    ret = realloc(ret, sizeof(char) * retsize);
    strcat(ret, buf);
    bzero(buf, sizeof(buf));

    if (affinity->nodemask != 0) {
        __mb_bits_to_string((char *) &affinity->nodemask,
                            sizeof(affinity->nodemask) * 8,
                            buf);
        retsize += strlen(buf) + 1;
        ret = realloc(ret, sizeof(char) * retsize);
        strcat(ret, ":");
        strcat(ret, buf);
    }

    return ret;
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
mb_rand_range_ulong(struct drand48_data* rand, unsigned long from, unsigned long to)
{
    unsigned long width = to - from;
    double ret;
    drand48_r(rand, &ret);
    return ret * width + from;
}

long
mb_rand_range_long(struct drand48_data* rand, long from, long to)
{
    long width = to - from;
    double ret;
    drand48_r(rand, &ret);
    return ret * width + from;
}

int64_t
mb_getsize(const char *path)
{
    int fd;
    int64_t size;
    struct stat statbuf;

    size = -1;

    if ((fd = open(path, O_RDONLY)) == -1){
        perror("Failed to open device or file\n");
        return -1;
    }
    if (fstat(fd, &statbuf) == -1){
        perror("fstat(2) failed.\n");
        goto finally;
    }

    if (S_ISREG(statbuf.st_mode)){
        size = statbuf.st_size;
        goto finally;
    }

    if (S_ISBLK(statbuf.st_mode)){
        uint64_t dev_sz;
        // if(ioctl(fd, BLKGETSIZE, &blk_num) == -1){
        //     perror("ioctl(BLKGETSIZE) failed\n");
        //     goto finally;
        // }
        // if(ioctl(fd, BLKSSZGET, &blk_sz) == -1){
        //     perror("ioctl(BLKSSZGET) failed\n");
        //     goto finally;
        // }
        if (ioctl(fd, BLKGETSIZE64, &dev_sz) == -1){
            perror("ioctl(BLKGETSIZE64) failed\n");
            goto finally;
        }

        // size = blk_num * blk_sz; // see <linux/fs.h>
        size = dev_sz;
    }

finally:
    close(fd);
    return size;
}
