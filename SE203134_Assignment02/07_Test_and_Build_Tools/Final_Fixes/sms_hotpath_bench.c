/* Evidence-only host benchmark: hot path analogous to SMS processing.
 * It is NOT the actual QEMU ARM sensor path nor the normal live sms_app.
 * Author: Tran Tuan Minh / SE203134 | 2026-10-27
 */
#include "circular_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
static volatile double sink;
__attribute__((noinline)) static double compute_rolling_average(const double *p)
{
    double s=0;
    for (unsigned int i=0;i<10;i++)s+=p[i];
    return s/10;
}
__attribute__((noinline)) static void process_sms_sample(CircularBuffer *b,uint64_t i)
{
    SensorData v={.timestamp_ms=i,.temperature=15+(i%3000)/100.0,
        .humidity=30+(i%6000)/100.0};
    SensorData out;
    double array[10];
    if (cb_push(b,v)==1 && cb_pop(b,&out)==1) {
        for (unsigned int j=0;j<10;j++) array[j]=out.temperature+(double)j;
        sink+=compute_rolling_average(array);
    }
}
int main(int argc,char **argv)
{
    const unsigned long n = argc>1 ? strtoul(argv[1],NULL,10) : 20000000ul;
    CircularBuffer b;
    if (cb_init(&b,64)!=0)return 1;
    for(unsigned long i=0;i<n;i++)process_sms_sample(&b,(uint64_t)i);
    cb_close(&b);
    cb_destroy(&b);
    printf("[HOST_HOTPATH] iterations=%lu sum=%f\n",n,sink);
    return 0;
}
