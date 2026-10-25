#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include "asgn1_ioctl.h"

#define DEVICE "/dev/asgn1"

static void print_stats(struct asgn1_stats *stats)
{
    printf("\n=== ASGN1 Statistics ===\n");
    printf("Open count      : %d\n", stats->open_count);
    printf("Write count     : %d\n", stats->write_count);
    printf("Read count      : %d\n", stats->read_count);
    printf("Buffer length   : %d\n", stats->buffer_len);
    printf("Last write size : %d\n", stats->last_write_size);
}

int main(void)
{
    int fd;
    int mode;
    char version[ASGN1_VERSION_SIZE];
    struct asgn1_stats stats;

    printf("========================================\n");
    printf("      ASGN1 IOCTL TEST PROGRAM\n");
    printf("========================================\n");

    /* Open device */
    fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open /dev/asgn1");
        return 1;
    }

    printf("\n[1] Device opened successfully\n");

    /*
     * GET_VERSION
     */
    memset(version, 0, sizeof(version));

    printf("\n[2] Testing ASGN1_GET_VERSION...\n");

    if (ioctl(fd, ASGN1_GET_VERSION, version) < 0) {
        perror("ASGN1_GET_VERSION");
        close(fd);
        return 1;
    }

    printf("Version: %s\n", version);

    /*
     * SET_MODE -> ECHO
     */
    printf("\n[3] Testing ASGN1_SET_MODE -> ECHO mode\n");

    mode = ASGN1_MODE_ECHO;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0) {
        perror("ASGN1_SET_MODE ECHO");
        close(fd);
        return 1;
    }

    printf("Mode set to ECHO (%d)\n", mode);

    /*
     * GET_STATS
     */
    printf("\n[4] Testing ASGN1_GET_STATS...\n");

    memset(&stats, 0, sizeof(stats));

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("ASGN1_GET_STATS");
        close(fd);
        return 1;
    }

    print_stats(&stats);

    /*
     * SET_MODE -> KERNEL PREFIX
     */
    printf("\n[5] Testing ASGN1_SET_MODE -> KERNEL mode\n");

    mode = ASGN1_MODE_KERNEL;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0) {
        perror("ASGN1_SET_MODE KERNEL");
        close(fd);
        return 1;
    }

    printf("Mode set to KERNEL (%d)\n", mode);

    /*
     * RESET_BUFFER
     */
    printf("\n[6] Testing ASGN1_RESET_BUFFER...\n");

    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) {
        perror("ASGN1_RESET_BUFFER");
        close(fd);
        return 1;
    }

    printf("Buffer reset successfully\n");

    /*
     * Check statistics after reset
     */
    printf("\n[7] Checking statistics after RESET...\n");

    memset(&stats, 0, sizeof(stats));

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("ASGN1_GET_STATS after RESET");
        close(fd);
        return 1;
    }

    print_stats(&stats);

    /*
     * Close device
     */
    close(fd);

    printf("\n========================================\n");
    printf("        IOCTL TEST COMPLETED\n");
    printf("========================================\n");

    return 0;
}
