#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "../common/mandelbrot.h"


typedef struct datathread{
    unsigned int offset;
    unsigned int points;
}datathread_t;

enum THREAD_CMD {READY,PLAY,STOP};
static enum THREAD_CMD *workstates = NULL;
static datathread_t *threaddata;
static int n_threads = 8;
static int n_workers = 0;
static int grain = 4;
static pthread_t *threads;
static pthread_mutex_t masterlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t *locks;
static pthread_cond_t mastercond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t *conds;
static complex_t julia_c = {JULIA_C_REAL, JULIA_C_IMAG};
static void (*function_p_calc)(complex_t *, int *, int, int, zoominfo_t);


void mandelbrot_calc(complex_t *mat, int *iterations, int idxoffset, int n_points, zoominfo_t zinfo){
    complex_t z, c;
    double rnew;
    int idxmat = idxoffset;
    int i = idxmat / width;
    int j = idxmat % width;
    while(idxmat < idxoffset + n_points){
        int iter = 0;
        z.r = 0; z.i = 0;
        c.r = ((double)(j - width/2 + zinfo.start_x))/zinfo.ratio;
        c.i = ((double)(i - height/2 + zinfo.start_y))/zinfo.ratio;
        while(iter < max_iter && (z.r*z.r + z.i*z.i) <= 4.0) {
            rnew = (z.r * z.r) - (z.i * z.i) + c.r;
            z.i = 2 * (z.r) * (z.i) + c.i;
            z.r = rnew;
            iter++;
        }
        mat[idxmat].r = z.r;
        mat[idxmat].i = z.i;
        if(iter == max_iter) iterations[idxmat] = -1;  //dentro insieme
        else iterations[idxmat] = iter;
        idxmat++;
        if(++j == width){
            j = 0;
            i++;
        }
    }
}

void julia_calc(complex_t *mat, int *iterations, int idxoffset, int n_points, zoominfo_t zinfo){
    complex_t z, c;
    double rnew;
    c = julia_c;
    int idxmat = idxoffset;
    int i = idxmat / width;
    int j = idxmat % width;
    while(idxmat < idxoffset + n_points){
        int iter = 0;
        z.r = ((double)(j - width/2 + zinfo.start_x))/zinfo.ratio;
        z.i = ((double)(i - height/2 + zinfo.start_y))/zinfo.ratio;
        while(iter < max_iter && (z.r*z.r + z.i*z.i) <= 4.0){
            rnew = (z.r * z.r) - (z.i * z.i) + c.r;
            z.i = 2 * (z.r) * (z.i) + c.i;
            z.r = rnew;
            iter++;
        }
        mat[idxmat].r = z.r;
        mat[idxmat].i = z.i;
        if(iter == max_iter) iterations[idxmat] = -1;  //dentro insieme
        else iterations[idxmat] = iter;
        idxmat++;
        if(++j == width){
            j = 0;
            i++;
        }
    }
}

//uso mutex per accedere alla variabile che mi salva l'ordine per il thread attuale
// Function executed by the POOL THREAD
void *run(void *data){
    int tid = (int)(long)data;
    while(TRUE){
        pthread_mutex_lock(&locks[tid]);
        workstates[tid] = READY;
        pthread_cond_wait(&conds[tid], &locks[tid]);
        if(workstates[tid] == STOP){
            pthread_mutex_unlock(&locks[tid]);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&locks[tid]);
        // no critical section
        //printf("[%d] offset: %ud, points: %ud\n", tid, threaddata[tid].offset, threaddata[tid].points);
        function_p_calc(matrix, iterations, threaddata[tid].offset, threaddata[tid].points, zoom_info);
        pthread_mutex_lock(&masterlock);
        n_workers--;
        //printf("[%d] exiting active %d\n", tid, n_workers);
        pthread_mutex_unlock(&masterlock);
        pthread_cond_broadcast(&mastercond);
    }
}


static inline void set_workstates(enum THREAD_CMD val){
    for(int i = 0; i < n_threads; i++)
        workstates[i] = val;
}


void core_init(void **args, char *msg){
    if(args[ARG_NUMP] != NULL) n_threads = (int)strtol((char*)args[ARG_NUMP], NULL, 10);
    if(args[ARG_GRAIN] != NULL) grain = (int)strtol((char*)args[ARG_GRAIN], NULL, 10);
    if(args[ARG_JULY]){
        function_p_calc = julia_calc;
        if(args[ARG_JULR] != NULL && args[ARG_JULI] != NULL){
            julia_c.r = strtod((char*)args[ARG_JULR], NULL);
            julia_c.i = strtod((char*)args[ARG_JULI], NULL);
        }
    }
    else{
        function_p_calc = mandelbrot_calc;
    }
    threads = (pthread_t*) malloc(sizeof(pthread_t) * n_threads);
    workstates = (THREAD_CMD*) malloc(sizeof(enum THREAD_CMD) * n_threads);
    threaddata = (datathread_t*) malloc(sizeof(datathread_t) * n_threads);
    locks = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t) * n_threads);
    conds = (pthread_cond_t*) malloc(sizeof(pthread_cond_t) * n_threads);
    for(long i = 0; i < n_threads; i++){
        locks[i] = PTHREAD_MUTEX_INITIALIZER;
        conds[i] = PTHREAD_COND_INITIALIZER;
        if(pthread_create(&threads[i], NULL, run, (void*)i) != 0){
            exit(1);
        }
    }
    sprintf(msg, "Threads %d Grain %d", n_threads, grain);
}

void core_computation(){
    int offset = 0;
    n_workers = 0;
    int size = width*height;
    int points = (size / grain) / n_threads;
    pthread_mutex_unlock(&masterlock);
    while(offset < size){
        for(int i=0; i<n_threads && offset<size; i++){
            pthread_mutex_lock(&locks[i]);
            if(workstates[i] == READY){
                threaddata[i].offset = offset;
                threaddata[i].points = (offset+points>size) ? size-offset : points;
                workstates[i] = PLAY;
                offset += points;
                pthread_mutex_lock(&masterlock);
                n_workers++;
                pthread_mutex_unlock(&masterlock);
                pthread_cond_broadcast(&conds[i]);
            }
            pthread_mutex_unlock(&locks[i]);
        }
        pthread_mutex_lock(&masterlock);
        if(n_workers > 0) pthread_cond_wait(&mastercond, &masterlock);
        pthread_mutex_unlock(&masterlock);
    }
    //printf("I'm here (active %d)\n", n_workers);
    pthread_mutex_lock(&masterlock);
    while(n_workers > 0)
        pthread_cond_wait(&mastercond, &masterlock); //printf("[root] active %d\n", n_workers);
    pthread_mutex_unlock(&masterlock);
}

void core_exit(void **data){
    set_workstates(STOP);
    for(int i = 0; i < n_threads; i++){
        pthread_cond_broadcast(&conds[i]);
        pthread_join(threads[i], NULL);
    }
    free(threads);
}
