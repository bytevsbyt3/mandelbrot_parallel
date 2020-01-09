#include <stdio.h>
#include "time_util.h"

#if USE_CLOCK_FUNCTION==1
static __inline__ void format_time(timedata_t *data, double time_result){
    data->h = (int)time_result / 3600;
    data->m = ((int)time_result / 60) - ( data->h * 60);
    data->s = (int)time_result % 60;
    data->milli = (int)(time_result * 1000) % 1000;
    data->micro = (int)(time_result * 1000000) % 1000;
    data->milli_ext = ( data->m * 60 + data->s) * 1000 + data->milli;
}
#else
static __inline__ void format_time(timedata_t *data, clocktype_t time_result){
    long us = time_result.tv_nsec / 1000;
    long ms = us / 1000;
    data->h = time_result.tv_sec / 3600;
    data->m = time_result.tv_sec / 60 - data->h * 60;
    data->s = time_result.tv_sec % 60;
    data->milli = ms;
    data->micro = us - (ms * 1000);
    data->milli_ext = (data->m * 60 + data->s) * 1000 + data->milli;
}
#endif

void print_time(clocktype_t time_result, const char * const msg){
    #if USE_CLOCK_FUNCTION==1
    printf("%s%lf\n", msg, time_result);
    #else
    printf("%s%ld.%ld  (ns)\n", msg, time_result.tv_sec, time_result.tv_nsec);
    #endif
    fflush(stdout);
}

void print_time_detailed(clocktype_t time_result, const char * const msg){
    timedata_t d;
    format_time(&d, time_result);
    printf("%s%dh:%dm:%02ds  %3dms %3dus\n", msg, d.h, d.m, d.s, d.milli, d.micro);
    fflush(stdout);
}
