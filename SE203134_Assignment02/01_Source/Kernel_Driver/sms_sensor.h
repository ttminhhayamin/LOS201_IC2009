/* Sensor Monitoring System ioctl API | Tran Tuan Minh - SE203134 | 2026-10-26 */
#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H
#include <linux/ioctl.h>
#define SMS_IOC_MAGIC 'S'
#define SMS_SET_INTERVAL _IOW(SMS_IOC_MAGIC, 1, unsigned int)
#endif
