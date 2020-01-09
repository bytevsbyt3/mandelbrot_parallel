#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>
#include "../common/mandelbrot.h"


static int nproc;
static int rank;
static int points_proc_max;
static void (*function_p_calc)(complex_t *, int *, int, int, zoominfo_t);
static complex_t julia_c = {JULIA_C_REAL, JULIA_C_IMAG};
static MPI_Datatype mpitype;

/* mat e iterations sono sottoporzioni della matrice originale */
void mandelbrot_calc(complex_t *mat, int iterations[], int idxoffset, int n_points, zoominfo_t zinfo){
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
        while(iter < max_iter && (z.r*z.r + z.i*z.i) <= 4.0){
            rnew = (z.r * z.r) - (z.i * z.i) + c.r;
            z.i = 2 * (z.r) * (z.i) + c.i;
            z.r = rnew;
            iter++;
        }
        mat[idxmat].r = z.r;
        mat[idxmat].i = z.i;
        if(iter == max_iter) iterations[idxmat] = -1; //dentro insieme
        else iterations[idxmat] = iter;
        idxmat++;
        if(++j == width){
            j = 0;
            i++;
        }
    }
}

void julia_calc(complex_t *mat, int iterations[], int idxoffset, int n_points, zoominfo_t zinfo){
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
        if(iter == max_iter) iterations[idxmat] = -1; //dentro insieme
        else iterations[idxmat] = iter;
        idxmat++;
        if(++j == width){
            j = 0;
            i++;
        }
    }
}

/* Little hack with the zoominfo, I use this to communicate if continue to work
 * or not to decrease the number of communication
 */
void mpi_process_core(){
    int size = width * height;
    int idxoffset, n_points;
    idxoffset = (rank-1) * points_proc_max;
    if(rank == nproc-1) n_points = size - idxoffset;
    else n_points = points_proc_max;
    //printf("[%d] npoints: %d, offset: %d\n", (rank-1), n_points, idxoffset);
    MPI_Bcast(&zoom_info, 1, mpitype, 0, MPI_COMM_WORLD);
    while(zoom_info.ratio){
        function_p_calc(matrix, iterations, idxoffset, n_points, zoom_info);
        MPI_Send(&iterations[idxoffset], n_points, MPI_INT, 0, 1, MPI_COMM_WORLD);
        //printf("send proc %d\n", rank);
        MPI_Bcast(&zoom_info, 1, mpitype, 0, MPI_COMM_WORLD);
    }
    free(iterations);
    free(matrix);
}


/* Build the zoom struct type and commit it */
void mpi_build_struct_type(MPI_Datatype *mpitype_ptr){
    int blocklengths[4] = {1,1,1,1};
    MPI_Datatype types[4] = {MPI_DOUBLE,MPI_DOUBLE,MPI_LONG,MPI_LONG};
    MPI_Aint displacements[4];
    MPI_Aint double1ex,double2ex,long1ex,long2ex,double1lb,double2lb,long1lb,long2lb;
    MPI_Type_get_extent(MPI_DOUBLE, &double1lb, &double1ex);
    MPI_Type_get_extent(MPI_DOUBLE, &double2lb, &double2ex);
    MPI_Type_get_extent(MPI_LONG, &long1lb, &long1ex);
    MPI_Type_get_extent(MPI_LONG, &long2lb, &long2ex);
    displacements[0] = double1lb;
    displacements[1] = double2lb + double1ex;
    displacements[2] = ((MPI_Aint)&(zoom_info.start_x)) - ((MPI_Aint)&zoom_info);
    displacements[3] = ((MPI_Aint)&(zoom_info.start_y)) - ((MPI_Aint)&zoom_info);
    MPI_Type_create_struct(4, blocklengths, displacements, types, mpitype_ptr);
    MPI_Type_commit(mpitype_ptr);
}

void core_init(void **args){
    int argc = (int)(long)args[ARG_ARGC];
    char **argv = (char **)args[ARG_ARGV];
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);
    //build the zoom struct type for mpi
    mpi_build_struct_type(&mpitype);
    //prows = height / (nproc-1);
    points_proc_max = (width*height) / (nproc-1);
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
    if(rank != 0){
        mpi_process_core();
        MPI_Finalize();
        exit(0);
    }
    //rank 0 return
    //iterations = (int*)malloc(sizeof(int) * width * height);
}

void core_computation(){
    MPI_Status status;
    MPI_Bcast(&zoom_info, 1, mpitype, 0, MPI_COMM_WORLD);
    int size = width * height;
    for(int i = 1; i < nproc; i++){
        //printf("prima....");
        int points;
        int idxoffset = (i-1) * points_proc_max;
        if(i == nproc-1) points = size - idxoffset;
        else points = points_proc_max;
        MPI_Recv(&iterations[idxoffset], points, MPI_INT, i, 1, MPI_COMM_WORLD, &status);
        //printf("recv from %d\n", status.MPI_SOURCE);
    }
}

void core_exit(void **data){
    zoom_info.ratio = 0;
    MPI_Bcast(&zoom_info, 1, mpitype, 0, MPI_COMM_WORLD);
    MPI_Finalize();
}




/* Backup parte nuova
//MPI_Request request;
for(int i=1; i<nproc; i++){
    if(offset_mem[i] >= 0){
        MPI_Irecv(&iterations[offset_mem[i]], points, MPI_INT, i, MPI_TAG_RESULT, MPI_COMM_WORLD, &request);
        MPI_Test(&request, &flag, &status);
        if(flag > -1 && status.MPI_SOURCE > 0 && status.MPI_SOURCE < nproc){
            MPI_Wait(&request, &status);
            offset_mem[i] = -1;
        }
    }
}*/
/*do{
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_TAG_RESULT, MPI_COMM_WORLD, &flag, &status);
    //printf("[root] Iprobe RESULT from %d (flag %d)\n",  status.MPI_SOURCE, flag);
    if(flag > -1 && status.MPI_SOURCE > 0){
        MPI_Recv(&iterations[offset_mem[status.MPI_SOURCE]], points, MPI_INT, status.MPI_SOURCE, MPI_TAG_RESULT, MPI_COMM_WORLD, &status);
        offset_mem[status.MPI_SOURCE] = -1;
        //printf("[root] TERMINATED %d \n", status.MPI_SOURCE);
    }
}while(flag > 0 && status.MPI_SOURCE > 0);*/
