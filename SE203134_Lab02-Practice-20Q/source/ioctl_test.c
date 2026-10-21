#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define LAB2_IOC_MAGIC 'L'
#define GET_BUFFER_LEN _IOR(LAB2_IOC_MAGIC, 1, int)
#define CLEAR_BUFFER   _IO(LAB2_IOC_MAGIC, 2)

int main(void)
{
    int fd;
    int len = -1;
    char data[200];
    ssize_t ret;

    fd = open("/dev/lab2", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    memset(data, 'A', sizeof(data));

    printf("Clearing buffer with ioctl...\n");
    if (ioctl(fd, CLEAR_BUFFER) < 0) {
        perror("CLEAR_BUFFER");
        close(fd);
        return 1;
    }

    printf("Single write requested: 200 bytes\n");

    ret = write(fd, data, sizeof(data));
    printf("write() returned       : %zd bytes\n", ret);

    if (ioctl(fd, GET_BUFFER_LEN, &len) < 0) {
        perror("GET_BUFFER_LEN");
        close(fd);
        return 1;
    }

    printf("GET_BUFFER_LEN         : %d bytes\n", len);

    printf("Clearing buffer again...\n");

    if (ioctl(fd, CLEAR_BUFFER) < 0) {
        perror("CLEAR_BUFFER");
        close(fd);
        return 1;
    }

    len = -1;

    if (ioctl(fd, GET_BUFFER_LEN, &len) < 0) {
        perror("GET_BUFFER_LEN");
        close(fd);
        return 1;
    }

    printf("Length after clear     : %d bytes\n", len);

    close(fd);
    return 0;
}
