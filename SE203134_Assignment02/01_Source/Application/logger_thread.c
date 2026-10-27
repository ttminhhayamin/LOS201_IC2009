/* Assignment02: Logger Thread (fail-fast I/O, timeout receive, 1MiB rotation).
 * Author: Tran Tuan Minh, SE203134 | Date: 2026-10-27
 * SE203134_FINAL_FIX_V1
 */
#include "sms_shared.h"
#include "logger_thread.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void logger_failed(const char *op)
{
    perror(op);
    atomic_store(&log_consumer_failed,1);
    atomic_store(&runtime_error,1);
    atomic_store(&stop_requested,1);
}

void *logger_thread(void *unused)
{
    (void)unused;
    char rotated[sizeof(LOG_PATH)+8];
    snprintf(rotated,sizeof(rotated),"%s.1",LOG_PATH);
    FILE *out=fopen(LOG_PATH,"a");
    if (!out) { logger_failed("open log"); return NULL; }
    char msg[MQ_TEXT_SIZE];
    time_t last_flush=time(NULL);
    int failed=0;
    for (;;) {
        struct timespec deadline;
        if (clock_gettime(CLOCK_REALTIME,&deadline)<0) {
            logger_failed("logger clock_gettime"); failed=1; break;
        }
        deadline.tv_sec++;
        ssize_t n=mq_timedreceive(log_queue,msg,sizeof(msg),NULL,&deadline);
        if (n>=0) {
            msg[(n<(ssize_t)sizeof(msg))?(size_t)n:sizeof(msg)-1]='\0';
            if (!strcmp(msg,SMS_QUEUE_STOP_MSG)) break;
            time_t wall=time(NULL);
            struct tm parts;
            char datebuf[40];
            int printed;
            if (localtime_r(&wall,&parts) &&
                strftime(datebuf,sizeof(datebuf),"%Y-%m-%d %H:%M:%S %z",&parts))
                printed=fprintf(out,"[%s] %s\n",datebuf,msg);
            else printed=fprintf(out,"%s\n",msg);
            if (printed<0) {logger_failed("logger write");failed=1;break;}
            long pos=ftell(out);
            if (pos<0) {logger_failed("logger ftell");failed=1;break;}
            if (pos>1024L*1024L) {
                if (fflush(out)!=0) {logger_failed("logger fflush before rotate");failed=1;break;}
                if (fclose(out)!=0) {
                    out=NULL;logger_failed("logger fclose before rotate");failed=1;break;
                }
                out=NULL;
                if (rename(LOG_PATH,rotated)!=0) {
                    logger_failed("logger rename rotation");failed=1;break;
                }
                out=fopen(LOG_PATH,"w");
                if (!out) {logger_failed("logger fopen after rotation");failed=1;break;}
            }
        } else if (errno!=ETIMEDOUT && errno!=EINTR) {
            logger_failed("logger mq_timedreceive");failed=1;break;
        }
        if (time(NULL)-last_flush>=5) {
            if (fflush(out)!=0) {logger_failed("logger periodic fflush");failed=1;break;}
            last_flush=time(NULL);
        }
    }
    if (out) {
        if (fflush(out)!=0 && !failed) logger_failed("logger final fflush");
        if (fclose(out)!=0 && !failed) logger_failed("logger final fclose");
    }
    return NULL;
}
