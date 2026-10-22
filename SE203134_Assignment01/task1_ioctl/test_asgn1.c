#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "asgn1_ioctl.h"

#define DEVICE "/dev/asgn1"

static int test_read(const char *expected)
{
    char buffer[128] = {0};
    int fd;
    ssize_t n;

    fd = open(DEVICE, O_RDONLY);

    if (fd < 0) {
        perror("open for read");
        return -1;
    }

    n = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (n < 0) {
        perror("read");
        return -1;
    }

    buffer[n] = '\0';

    printf("Read result: [%s]\n", buffer);

    if (strcmp(buffer, expected) != 0) {
        printf("FAIL: expected [%s]\n", expected);
        return -1;
    }

    return 0;
}

int main(void)
{
    int fd;
    int mode;

    char version[ASGN1_VERSION_SIZE] = {0};
    struct asgn1_stats stats;

    fd = open(DEVICE, O_RDWR);

    if (fd < 0) {
        perror("open device");
        return 1;
    }

    printf("=== ASSIGNMENT 01 IOCTL TEST ===\n");

    printf("\n[1] Testing GET_VERSION\n");

    if (ioctl(fd, ASGN1_GET_VERSION, version) < 0) {
        perror("GET_VERSION");
        goto fail;
    }

    printf("Driver version: %s\n", version);

    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) {
        perror("RESET_BUFFER");
        goto fail;
    }

    printf("\n[2] Testing ECHO MODE\n");

    mode = ASGN1_MODE_ECHO;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0) {
        perror("SET_MODE ECHO");
        goto fail;
    }

    if (write(fd, "Hello", 5) != 5) {
        perror("write echo");
        goto fail;
    }

    if (test_read("Hello") < 0)
        goto fail;

    printf("\n[3] Testing KERNEL MODE\n");

    mode = ASGN1_MODE_KERNEL;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0) {
        perror("SET_MODE KERNEL");
        goto fail;
    }

    if (write(fd, "World", 5) != 5) {
        perror("write kernel");
        goto fail;
    }

    if (test_read("[KERNEL]World") < 0)
        goto fail;

    printf("\n[4] Testing GET_STATS\n");

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("GET_STATS");
        goto fail;
    }

    printf("Open count      : %d\n", stats.open_count);
    printf("Write count     : %d\n", stats.write_count);
    printf("Read count      : %d\n", stats.read_count);
    printf("Buffer length   : %d\n", stats.buffer_len);
    printf("Last write size : %d\n", stats.last_write_size);

    printf("\n[5] Testing RESET_BUFFER\n");

    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) {
        perror("RESET_BUFFER");
        goto fail;
    }

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("GET_STATS after reset");
        goto fail;
    }

    printf("Buffer length after reset : %d\n",
           stats.buffer_len);

    printf("Read count after reset    : %d\n",
           stats.read_count);

    printf("Write count after reset   : %d\n",
           stats.write_count);

    if (stats.buffer_len != 0 ||
        stats.read_count != 0 ||
        stats.write_count != 0) {
        printf("FAIL: Reset did not clear statistics\n");
        goto fail;
    }

    if (test_read("") < 0)
        goto fail;

    printf("\n=== ALL IOCTL TESTS PASSED ===\n");

    close(fd);
    return 0;

fail:
    printf("\n=== TEST FAILED ===\n");
    close(fd);
    return 1;
}
