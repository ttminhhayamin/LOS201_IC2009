/* Real ARM kernel driver test. Author: Tran Tuan Minh / SE203134 */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "sms_sensor.h"
int main(void) {
    int failures = 0, fd = open("/dev/sms_sensor", O_RDONLY);
    unsigned int interval = 500;
    char b[120];
    ssize_t n;
    if (fd < 0) {perror("open /dev/sms_sensor"); return 2;}
    if (ioctl(fd, SMS_SET_INTERVAL, &interval) < 0) {
        perror("VALID ioctl 500"); failures++;
    } else puts("[OK] ioctl 500ms");
    interval = 0;
    errno = 0;
    if (ioctl(fd, SMS_SET_INTERVAL, &interval) == -1 && errno == EINVAL)
        puts("[OK] ioctl 0 -> EINVAL");
    else { puts("[FAIL] invalid ioctl 0"); failures++; }
    errno = 0;
    if (ioctl(fd, 0xDEADBEEFul, &interval) == -1 && errno == ENOTTY)
        puts("[OK] unsupported ioctl -> ENOTTY");
    else { puts("[FAIL] unsupported ioctl"); failures++; }
    n = read(fd, b, sizeof(b)-1);
    if (n > 0) {b[n]=0; printf("[OK] read %ld bytes: %s",(long)n,b);}
    else { printf("[FAIL] read returned %ld errno=%d\n",(long)n,errno);failures++;}
    errno = 0;
    n = read(fd, b, sizeof(b));
    if (n == 0) puts("[OK] EOF second read same descriptor");
    else {puts("[WARN] unexpected second read result");}
    if (close(fd)) {perror("close"); failures++;}
    fd = open("/dev/sms_sensor", O_RDONLY);
    if (fd >= 0) {
        errno = 0; n = read(fd,b,sizeof(b));
        if (n == -1 && errno == EAGAIN) puts("[OK] immediate second open -> EAGAIN");
        else printf("[INFO] second open/read returned %ld, errno=%d (timing dependent)\n",(long)n,errno);
        close(fd);
    }
    printf("DRIVER TEST %s\n",failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
