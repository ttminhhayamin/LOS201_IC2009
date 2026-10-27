/* Assignment 02 alert_thread | SE203134 | 2026-10-27 */
#include "sms_shared.h"
#include "alert_thread.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void *alert_thread(void *unused)
{
    (void)unused;
    char msg[MQ_TEXT_SIZE];
    for (;;) {
        ssize_t n=mq_receive(alert_queue,msg,sizeof(msg),NULL);
        if (n >= 0) {
            msg[(n<(ssize_t)sizeof(msg))?(size_t)n:sizeof(msg)-1]='\0';
            if (strcmp(msg,SMS_QUEUE_STOP_MSG)==0) break;
            fprintf(stderr,"%s\n",msg);
            if (strstr(msg,"TEMP_HIGH")) alert_temp_high++;
            else if (strstr(msg,"HUMID_HIGH")) alert_humid_high++;
            else if (strstr(msg,"TEMP_SPIKE")) alert_spike++;
            mq_send_text(log_queue,msg);
        } else if (errno!=EINTR) {
            perror("alert mq_receive");
            atomic_store(&alert_consumer_failed,1);
            atomic_store(&runtime_error,1);
            atomic_store(&stop_requested,1);
            break;
        }
    }
    return NULL;
}
