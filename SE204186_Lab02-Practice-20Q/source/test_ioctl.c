#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "asgn1_ioctl.h"

#define DEVICE_PATH "/dev/asgn1"

int main(void)
{
    int fd;
    int mode;
    char version[64] = {0};
    char write_buf[] = "Hello IOCTL";
    char read_buf[128] = {0};
    struct asgn1_stats stats;
    ssize_t bytes;

    fd = open(DEVICE_PATH, O_RDWR);

    if (fd < 0) {
        perror("open /dev/asgn1");
        return 1;
    }

    printf("Device opened successfully.\n\n");

    /* Test 1: GET_VERSION */
    printf("TEST 1: ASGN1_GET_VERSION\n");

    if (ioctl(fd, ASGN1_GET_VERSION, version) < 0) {
        perror("ASGN1_GET_VERSION");
        close(fd);
        return 1;
    }

    printf("Driver version: %s\n\n", version);

    /* Test 2: SET_MODE */
    printf("TEST 2: ASGN1_SET_MODE\n");

    mode = 0;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0) {
        perror("ASGN1_SET_MODE");
        close(fd);
        return 1;
    }

    printf("Mode changed to %d.\n\n", mode);

    /* Test 3: RESET_BUFFER */
    printf("TEST 3: ASGN1_RESET_BUFFER\n");

    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) {
        perror("ASGN1_RESET_BUFFER");
        close(fd);
        return 1;
    }

    printf("Buffer and read/write statistics reset.\n\n");

    /* Write data */
    printf("Writing data: %s\n", write_buf);

    bytes = write(fd, write_buf, strlen(write_buf));

    if (bytes < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    printf("Written %zd bytes.\n", bytes);

    /* Read data */
printf("\nReading data:\n");

close(fd);

fd = open(DEVICE_PATH, O_RDWR);
if (fd < 0) {
    perror("open");
    return 1;
}

bytes = read(fd, read_buf, sizeof(read_buf) - 1);
if (bytes < 0) {
    perror("read");
    close(fd);
    return 1;
}

read_buf[bytes] = '\0';
printf("Read %zd bytes: [%s]\n\n", bytes, read_buf);


    /* Test 4: GET_STATS */
    printf("TEST 4: ASGN1_GET_STATS\n");

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("ASGN1_GET_STATS");
        close(fd);
        return 1;
    }

    printf("Open count       : %d\n", stats.open_count);
    printf("Write count      : %d\n", stats.write_count);
    printf("Read count       : %d\n", stats.read_count);
    printf("Buffer length    : %d\n", stats.buffer_len);
    printf("Last write size  : %d\n\n", stats.last_write_size);

    /* Reset buffer again */
    printf("Resetting buffer again...\n");

    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) {
        perror("ASGN1_RESET_BUFFER");
        close(fd);
        return 1;
    }

    printf("Final statistics:\n");

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("ASGN1_GET_STATS");
        close(fd);
        return 1;
    }

    printf("Open count       : %d\n", stats.open_count);
    printf("Write count      : %d\n", stats.write_count);
    printf("Read count       : %d\n", stats.read_count);
    printf("Buffer length    : %d\n", stats.buffer_len);
    printf("Last write size  : %d\n", stats.last_write_size);

    close(fd);

    printf("\nAll IOCTL tests completed.\n");

    return 0;
}
