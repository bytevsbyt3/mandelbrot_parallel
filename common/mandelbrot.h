#ifndef __MANDELBROT_H_BVB2019__
#define __MANDELBROT_H_BVB2019__

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef GETID_MAT_FUNC_PREFIX
#define GETID_MAT_FUNC_PREFIX   // let empty and redefine only if needed (gpu)
#endif
#define JULIA_C_REAL -1.0
#define JULIA_C_IMAG 0.0
#define BG_R 0
#define BG_G 0
#define BG_B 0
#define COLOR_B 0x000000ff
#define COLOR_G 0x0000ff00
#define COLOR_R 0x00ff0000
#define COLOR_RGB 0x00ffffff
#define ARG_ARGC 0      // Argc from main
#define ARG_ARGV 1      // Argv from main
#define ARG_JULY 2      // Julia set instead mandelbrot
#define ARG_JULR 3      // Julia real part
#define ARG_JULI 4      // Julia imaginary part
#define ARG_DRAW 5      // Paint mandelbrot on gtk window after textual sequence
#define ARG_NUMP 6      // Number of process / threads
#define ARG_ITER 7      // Max number of iterations per pixel
#define ARG_TEXT 8      // Textual mode (no gtk window) useful for benchmark
#define ARG_GRAIN 9


typedef struct complex{
    double r;
    double i;
}complex_t;

typedef struct zoominfo{
    double ratio;
    long start_x;
    long start_y;
    unsigned int move_px;
}zoominfo_t;

typedef struct rgbdouble{
    double r;
    double g;
    double b;
}rgbdouble_t;

typedef unsigned int matid_t;

GETID_MAT_FUNC_PREFIX static __inline__ matid_t getid(matid_t y, matid_t x, int width){
    return (y * width) + x;
}

// Global variables bound to main core module. There are too many differences
// in parallel implementations to use function parameters everywhere
extern int width;
extern int height;
extern int max_iter;
extern int *iterations;
extern complex_t *matrix;
extern zoominfo_t zoom_info;

#endif
