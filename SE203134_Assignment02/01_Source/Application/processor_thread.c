/* Assignment 02 processor_thread | SE203134 | 2026-10-27 */
#include "sms_shared.h"
#include "processor_thread.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void *processor_thread(void *unused)
{
    (void)unused;
    SensorData x, last={0};
    int have_last=0;
    double temp_history[10]={0}, humid_history[10]={0};
    unsigned int index=0, filled=0;
    while (cb_pop(&shared_buffer, &x)) {
        char msg[MQ_TEXT_SIZE];
        temp_history[index]=x.temperature;
        humid_history[index]=x.humidity;
        index=(index+1)%10;
        if (filled<10) filled++;
        double avg_t=0, avg_h=0;
        for (unsigned int k=0;k<filled;k++) {
            avg_t+=temp_history[k]; avg_h+=humid_history[k];
        }
        avg_t/=filled; avg_h/=filled;
        snprintf(msg, sizeof(msg), "[DATA][%" PRIu64 "] temp=%.2f humid=%.2f avg_t=%.2f avg_h=%.2f",
                 x.timestamp_ms,x.temperature,x.humidity,avg_t,avg_h);
        mq_send_text(log_queue,msg);
        if (x.temperature > temp_threshold) {
            snprintf(msg,sizeof(msg),"[ALERT][%" PRIu64 "] TEMP_HIGH: VALUE=%.2f THRESHOLD=%.2f",
                     x.timestamp_ms,x.temperature,temp_threshold);
            mq_send_text(alert_queue,msg);
        }
        if (x.humidity > humid_threshold) {
            snprintf(msg,sizeof(msg),"[ALERT][%" PRIu64 "] HUMID_HIGH: VALUE=%.2f THRESHOLD=%.2f",
                     x.timestamp_ms,x.humidity,humid_threshold);
            mq_send_text(alert_queue,msg);
        }
        if (have_last && (x.temperature-last.temperature > 5.0 ||
                          last.temperature-x.temperature > 5.0)) {
            snprintf(msg,sizeof(msg),"[ALERT][%" PRIu64 "] TEMP_SPIKE: VALUE=%.2f THRESHOLD=5.00",
                     x.timestamp_ms, x.temperature);
            mq_send_text(alert_queue,msg);
        }
        last=x; have_last=1;
    }
    return NULL;
}
