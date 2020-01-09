#ifndef __TIME_UTIL_H_BVB2019__
#define __TIME_UTIL_H_BVB2019__

#include <time.h>
/*
  In glibc 2.17 and earlier, clock() was implemented on top of times(2).
  For improved accuracy, since glibc 2.18, it is implemented on top of clock_gettime(2)
  (using the CLOCK_PROCESS_CPUTIME_ID clock).
*/

#ifndef USE_CLOCK_FUNCTION
#define USE_CLOCK_FUNCTION 0
#endif

#if USE_CLOCK_FUNCTION==1
typedef double clocktype_t;
#else
typedef struct timespec clocktype_t;
#endif

void print_time(clocktype_t tres, const char * const msg);
void print_time_detailed(clocktype_t tres, const char * const msg);

typedef struct {
    int h;
    int m;
    int s;
    int milli;
    int micro;
    int milli_ext;
} timedata_t;


#if USE_CLOCK_FUNCTION==1

static clock_t tstart;  // MUST BE GLOBAL
static __inline__ void clock_time_start(){
    tstart = clock();
}
static __inline__ double clock_time_stop(){
    return (double)(clock() - tstart)/(double)CLOCKS_PER_SEC;
}
static __inline__ clocktype_t clock_time_add(clocktype_t told, clocktype_t tadd){
    return told + tadd;
}
static __inline__ clocktype_t clock_time_init(){
    return 0.0;
}

#else

static clocktype_t tstart;  // MUST BE GLOBAL
static __inline__ void clock_time_start(){
    clock_gettime(CLOCK_MONOTONIC, &tstart);
}
static __inline__ clocktype_t clock_time_stop(){
    clocktype_t tstop;
    clock_gettime(CLOCK_MONOTONIC, &tstop);
    if(tstop.tv_nsec < tstart.tv_nsec){
        tstop.tv_sec  = tstop.tv_sec - tstart.tv_sec - 1;
        tstop.tv_nsec = 1000000000 + tstop.tv_nsec - tstart.tv_nsec;
    }
    else{
        tstop.tv_sec  -= tstart.tv_sec;
        tstop.tv_nsec -= tstart.tv_nsec;
    }
    return tstop;
}
static __inline__ clocktype_t clock_time_add(clocktype_t told, clocktype_t tadd){
    if(tadd.tv_nsec + told.tv_nsec > 1000000000){
        tadd.tv_sec  = tadd.tv_sec + told.tv_sec + 1;
        tadd.tv_nsec = tadd.tv_nsec + told.tv_nsec - 1000000000;
    }
    else{
        tadd.tv_sec += told.tv_sec;
        tadd.tv_nsec += told.tv_nsec;
    }
    return tadd;
}
static __inline__ clocktype_t clock_time_init(){
    clocktype_t t = {0, 0};
    return t;
}

#endif


#endif
