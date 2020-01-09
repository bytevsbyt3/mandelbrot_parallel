#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../common/mandelbrot.h"


static void (*function_p_calc)(complex_t *, int *, int, int, int, zoominfo_t);
static complex_t julia_c = {JULIA_C_REAL, JULIA_C_IMAG};

void mandelbrot_calc(complex_t *mat, int *iterpx, int max, int width, int height, zoominfo_t zinfo){
    complex_t z, c;
    double rnew;
    int iter, idxmat;
    for(int i = 0; i < height; i++){
        for(int j = 0; j < width; j++){
            iter = 0;
            idxmat = getid(i,j,width);
            z.r = 0; z.i = 0;
            c.r = ((double)(j - width/2 + zinfo.start_x))/zinfo.ratio;
            c.i = ((double)(i - height/2 + zinfo.start_y))/zinfo.ratio;
            while(iter < max && (z.r*z.r + z.i*z.i) <= 4.0){
                rnew = (z.r * z.r) - (z.i * z.i) + c.r;
                z.i = 2 * (z.r) * (z.i) + c.i;
                z.r = rnew;
                iter++;
            }
            mat[idxmat].r = z.r;
            mat[idxmat].i = z.i;
            if(iter == max) iterpx[idxmat] = -1;
            else iterpx[idxmat] = iter;
        }
    }
}

void julia_calc(complex_t *mat, int *iterpx, int max, int width, int height, zoominfo_t zinfo){
    complex_t z, c;
    double rnew;
    int iter, idxmat;
    c = julia_c;
    for(int i = 0; i < height; i++){
        for(int j = 0; j < width; j++){
            iter = 0;
            idxmat = getid(i,j,width);
            z.r = ((double)(j - width/2 + zinfo.start_x))/zinfo.ratio;
            z.i = ((double)(i - height/2 + zinfo.start_y))/zinfo.ratio;
            while(iter < max && (z.r*z.r + z.i*z.i) <= 4.0){
                rnew = (z.r * z.r) - (z.i * z.i) + c.r;
                z.i = 2 * (z.r) * (z.i) + c.i;
                z.r = rnew;
                iter++;
            }
            mat[idxmat].r = z.r;
            mat[idxmat].i = z.i;
            if(iter == max) iterpx[idxmat] = -1;  //dentro insieme
            else iterpx[idxmat] = iter;
        }
    }
}


void core_init(void **args, char *msg){
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
    sprintf(msg, "Sequential");
}

void core_computation(){
    function_p_calc(matrix, iterations, max_iter, width, height, zoom_info);
}

void core_exit(void **data){
    //NOTHING
}
