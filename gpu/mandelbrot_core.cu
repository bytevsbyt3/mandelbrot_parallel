#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define GETID_MAT_FUNC_PREFIX __device__
#include "../common/mandelbrot.h"


void (*kernel)(complex_t*,int*,int,int,int,int,int,zoominfo_t);

static int tpb = 32;           // Threads per block
static int n_points = 1;       // Points per thread
static int *d_iter;            // Device output buffer for iterations per point
static complex_t *d_mat;       // Device unidimensional buffer for data

__device__
complex_t julia_c = {JULIA_C_REAL, JULIA_C_IMAG};


__global__
void kernel_mandelbrot(complex_t *mat, int *iters, int maxiter, int size, int rows, int cols, int n_points, zoominfo_t zinfo){
    int index = threadIdx.x + blockIdx.x * blockDim.x;
    //if(index >= size)
    //    return;
    complex_t z, c;
    double rnew;
    long idxmat = ((long)index) * n_points;
    int i, j, iteration;
    while(idxmat < size && n_points > 0){
        iteration = 0;
        i = idxmat / cols;
        j = idxmat % cols;
        z.r = 0; z.i = 0;
        c.r = ((double)(j - cols/2 + zinfo.start_x))/zinfo.ratio;
        c.i = ((double)(i - rows/2 + zinfo.start_y))/zinfo.ratio;
        while(iteration < maxiter && (z.r*z.r + z.i*z.i) <= 4.0){
            rnew = (z.r * z.r) - (z.i * z.i) + c.r;
            z.i = 2 * (z.r) * (z.i) + c.i;
            z.r = rnew;
            iteration++;
        }
        mat[idxmat].r = z.r;
        mat[idxmat].i = z.i;
        if(iteration == maxiter) iters[idxmat] = -1;  //dentro insieme
        else iters[idxmat] = iteration;
        --n_points;
        ++idxmat;
    }
}


__global__
void kernel_julia(complex_t *mat, int *iters, int maxiter, int size, int rows, int cols, int n_points, zoominfo_t zinfo){
    int index = threadIdx.x + blockIdx.x * blockDim.x;
    //if(index >= size)
    //    return;
    complex_t z, c;
    double rnew;
    long idxmat = ((long)index) * n_points;
    int i, j, iteration;
    c = julia_c;
    while(idxmat < size && n_points > 0){
        iteration = 0;
        i = idxmat / cols;
        j = idxmat % cols;
        z.r = ((double)(j - cols/2 + zinfo.start_x))/zinfo.ratio;
        z.i = ((double)(i - rows/2 + zinfo.start_y))/zinfo.ratio;
        while(iteration < maxiter && (z.r*z.r + z.i*z.i) <= 4.0){
            rnew = (z.r * z.r) - (z.i * z.i) + c.r;
            z.i = 2 * (z.r) * (z.i) + c.i;
            z.r = rnew;
            iteration++;
        }
        mat[getid(i,j,cols)].r = z.r;
        mat[getid(i,j,cols)].i = z.i;
        if(iteration == maxiter) iters[idxmat] = -1;  //dentro insieme
        else iters[idxmat] = iteration;
        --n_points;
        ++idxmat;
    }
}

//if(matsize % n_points != 0) ++dim;  // fix es. per 58799
// (matsize+tpb-1)/tpb
void core_computation(){
    int matsize = width * height;
    int dim = matsize / n_points;
    if(matsize % n_points != 0)
        dim++;
    dim3 dimGrid( (dim+tpb-1)/tpb, 1, 1);
    cudaError_t err_sync, err_asyn;
    kernel<<<dimGrid, tpb>>>(d_mat, d_iter, max_iter, matsize, height, width, n_points, zoom_info);
    err_sync = cudaMemcpy(iterations, d_iter, matsize*sizeof(int), cudaMemcpyDeviceToHost);
    err_asyn = cudaGetLastError();
    if(err_sync != cudaSuccess)
        printf("memcpy error: %s\n", cudaGetErrorString(err_sync));
    else if(err_asyn != err_sync)
        printf("async. error: %s\n", cudaGetErrorString(err_asyn));
}

int checkCudaParameters(int matsize){
    int n_devices;
    int dimGrid = (matsize+tpb-1) / tpb;
    cudaDeviceProp prop;
    cudaGetDeviceCount(&n_devices);
    if(n_devices == 0){
        printf("no cuda device found\n");
        return 1;
    }
    cudaGetDeviceProperties(&prop, 0);
    if(tpb > prop.maxThreadsPerBlock){
        printf("%d threads per block is your limit\n", prop.maxThreadsPerBlock);
        return 1;
    }
    else if(tpb % prop.warpSize != 0){
        printf("block size is not a multiple of the warp size.. \n");
    }
    if(dimGrid > prop.maxGridSize[0]){
        printf("grid dim exceeds your limit, ");
        while(dimGrid > prop.maxGridSize[0]){
            n_points <<= 1;     // n_points*2
            int dim = matsize / n_points;
            if(matsize % n_points != 0) ++dim;
            dimGrid = (dim+tpb-1)/tpb;
        }
        printf("each thread must compute %d points\n", n_points);
    }
    return 0;
}

void core_init(void **args, char *msg){
    int size = width * height;
    if(args[ARG_NUMP] != NULL)
        tpb = (int)strtol((char*)args[ARG_NUMP], NULL, 10);
    if(checkCudaParameters(size)){
        exit(1);
    }
    cudaMalloc(&d_mat, size * sizeof(complex_t));
    cudaMalloc(&d_iter, size * sizeof(int));
    if(args[ARG_JULY]){
        kernel = kernel_julia;
        if(args[ARG_JULR] != NULL && args[ARG_JULI] != NULL){
            complex_t point;
            point.r = strtod((char*)args[ARG_JULR], NULL);
            point.i = strtod((char*)args[ARG_JULI], NULL);
            cudaMemcpyToSymbol(julia_c, &point, sizeof(complex_t), 0, cudaMemcpyHostToDevice);
        }
    }
    else{
        kernel = kernel_mandelbrot;
    }
    sprintf(msg, "GPU threads per block %d", tpb);
}

void core_exit(void **data){
    cudaFree(d_mat);
    cudaFree(d_iter);
}
