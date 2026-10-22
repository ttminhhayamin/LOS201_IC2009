#ifndef ASGN1_IOCTL_H
#define ASGN1_IOCTL_H

#include <linux/ioctl.h>

/* Magic number identifying our ioctl commands */
#define ASGN1_IOC_MAGIC 'A'

/* Driver statistics */
struct asgn1_stats {
    int open_count;
    int write_count;
    int read_count;
    int buffer_len;
    int last_write_size;
};

/* Driver operating modes */
#define ASGN1_MODE_KERNEL 0
#define ASGN1_MODE_ECHO   1

/* Maximum version string buffer size */
#define ASGN1_VERSION_SIZE 64

/* Command 1: Reset buffer and counters */
#define ASGN1_RESET_BUFFER \
    _IO(ASGN1_IOC_MAGIC, 0)

/* Command 2: Get driver statistics */
#define ASGN1_GET_STATS \
    _IOR(ASGN1_IOC_MAGIC, 1, struct asgn1_stats)

/* Command 3: Set operating mode */
#define ASGN1_SET_MODE \
    _IOW(ASGN1_IOC_MAGIC, 2, int)

/* Command 4: Get driver version */
#define ASGN1_GET_VERSION \
    _IOR(ASGN1_IOC_MAGIC, 3, char[ASGN1_VERSION_SIZE])

#endif
