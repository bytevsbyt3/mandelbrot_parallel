#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>
#include "../common/mandelbrot.h"


#define MPI_TAG_GETJOB 1
#define MPI_TAG_PARAMS 2
#define MPI_TAG_RESULT 3

static int nproc = 8;
static int grain = 4;
static int rank;
static void (*function_p_calc)(complex_t *, int *, int, int, zoominfo_t);
static complex_t julia_c = {JULIA_C_REAL, JULIA_C_IMAG};
static MPI_Datatype zoomtype;
static MPI_Datatype datatype;

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

/*
 * Method executed by each slave process
 * Little hack with the zoominfo, I use this to communicate if continue to work
 * or not to decrease the number of communication
 */
void mpi_process_core(){
    MPI_Status status;
    int data[2] = {0,1};   //data[0] offset
    MPI_Bcast(&zoom_info, 1, zoomtype, 0, MPI_COMM_WORLD);  //Blocking
    while(zoom_info.ratio){
        while(TRUE){
            MPI_Send(&iterations[data[0]], data[1], MPI_INT, 0, MPI_TAG_RESULT, MPI_COMM_WORLD);
            MPI_Recv(&data, 1, datatype, 0, MPI_TAG_PARAMS, MPI_COMM_WORLD, &status);
            //printf("[%d] received offset %d points %d\n", rank, data[0], data[1]);
            if(data[0] == -1)
                break;
            function_p_calc(matrix, iterations, data[0], data[1], zoom_info);
        }
        MPI_Bcast(&zoom_info, 1, zoomtype, 0, MPI_COMM_WORLD);
    }
    free(iterations);
    free(matrix);
}


void core_init(void **args, char *msg){
    int argc = (int)(long)args[ARG_ARGC];
    char **argv = (char **)args[ARG_ARGV];
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);
    //build the zoom struct type for mpi
    int blockleng[4] = {1,1,1,1};
    MPI_Aint disp[4];   //displacements
    MPI_Datatype types[4] = {MPI_DOUBLE, MPI_LONG, MPI_LONG, MPI_UNSIGNED};
    MPI_Get_address(&zoom_info.ratio, &disp[0]);
    MPI_Get_address(&zoom_info.start_x, &disp[1]);
    MPI_Get_address(&zoom_info.start_y, &disp[2]);
    MPI_Get_address(&zoom_info.move_px, &disp[3]);
    disp[3]-=disp[0];  disp[2]-=disp[0]; disp[1]-=disp[0]; disp[0] = 0;
    MPI_Type_create_struct(4, blockleng, disp, types, &zoomtype);
    MPI_Type_commit(&zoomtype);
    MPI_Type_contiguous(2, MPI_INT, &datatype);
    MPI_Type_commit(&datatype);
    //prows = height / (nproc-1);
    if(args[ARG_GRAIN] != NULL)
        grain = (int)strtol((char*)args[ARG_GRAIN], NULL, 10);
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
    sprintf(msg, "MPI Processes %d Grain %d", nproc, grain);
}


// Only MASTER execute this. Slave executes mpi_process_core
void core_computation(){
    MPI_Status status;
    int size = width * height;
    int points = (size / grain) / (nproc-1);
    int data[2];
    int offset = 0;
    int count = 0;
    int *offset_mem = (int*) malloc(sizeof(int) * nproc);
    int *buff = (int*)malloc(sizeof(int) * points);
    memset(offset_mem, -1, sizeof(int)*nproc);
    MPI_Bcast(&zoom_info, 1, zoomtype, 0, MPI_COMM_WORLD);  // Start synchronization
    while(offset < size){
        // Slave request
        MPI_Recv(buff, points, MPI_INT, MPI_ANY_SOURCE, MPI_TAG_RESULT, MPI_COMM_WORLD, &status);
        data[0] = offset;
        data[1] = (offset+points>size) ? size-offset : points;
        MPI_Send(&data, 1, datatype, status.MPI_SOURCE, MPI_TAG_PARAMS, MPI_COMM_WORLD);
        MPI_Get_count(&status, MPI_INT, &count);
        memcpy(&iterations[offset_mem[status.MPI_SOURCE]], buff, sizeof(int)*count);
        //printf("[root] recv from %d (count %d), new job sent (offset %d, points %d)\n", status.MPI_SOURCE, count, data[0], data[1]);
        offset_mem[status.MPI_SOURCE] = offset;
        offset += points;
    }
    data[0] = -1;
    for(int i=1; i<nproc; i++){
        if(offset_mem[i] >= 0){
            //printf("[root] MUST RECEIVE FROM %d\n", i);
            MPI_Recv(&iterations[offset_mem[i]], points, MPI_INT, i, MPI_TAG_RESULT, MPI_COMM_WORLD, &status); // Richiesta dati
        }
        MPI_Send(&data, 1, datatype, i, MPI_TAG_PARAMS, MPI_COMM_WORLD);
    }
    free(offset_mem);
    free(buff);
}

void core_exit(void **data){
    zoom_info.ratio = 0;
    MPI_Bcast(&zoom_info, 1, zoomtype, 0, MPI_COMM_WORLD);
    MPI_Finalize();
}
