#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "../common/mandelbrot.h"
#include "../common/time_util.h"
//#include <gtk/gtk.h>  MOVED DOWN


extern void core_init(void**, char*);
extern void core_computation();
extern void core_exit(void**);


int width = 0;
int height = 0;
int max_iter = 0;
int *iterations;
complex_t *matrix = NULL;
zoominfo_t zoom_info = {200, 0, 0, 10};
static clocktype_t time_sum = clock_time_init();
//static cairo_surface_t *mysurface = NULL; MOVED DOWN

static int zoom_adjust_c(char c){
    if(c=='r' || c=='R')    { zoom_info.start_x += zoom_info.move_px; }
    else if(c=='l' || c=='L'){ zoom_info.start_x -= zoom_info.move_px; }
    else if(c=='u' || c=='U'){ zoom_info.start_y -= zoom_info.move_px; }
    else if(c=='d' || c=='D'){ zoom_info.start_y += zoom_info.move_px; }
    else if(c=='z' || c=='Z'){
        zoom_info.ratio *= 2;
        zoom_info.start_x *= 2;
        zoom_info.start_y *= 2;
    }
    else if(c=='b' || c=='B'){
        zoom_info.ratio /= 2;
        zoom_info.start_x /= 2;
        zoom_info.start_y /= 2;
    }
    else if(c == 'x'){}
    else{ return -1; }
    return 0;
}

static char*** read_configs(){
    char line[100];
    int i = 0;
    FILE *file;
    char ***configs = (char***) malloc(sizeof(char**) * 16);
    memset(configs, 0, sizeof(char**) * 16);
    if( (file=fopen("config.ini", "r")) == NULL ){
        perror("lettura config file");
        exit(1);
    }
    while(fgets(line, 100, file) != NULL){
        if(line[0] != '#'){
            line[99] = '\0';
            configs[i] = (char**) malloc(sizeof(char *) * 2);
            configs[i][0] = strdup(strtok(line, "="));
            configs[i][1] = strdup(strtok(NULL, ""));
            *strchr(configs[i][1],'\n') = '\0';
        }
        i++;
    }
    fclose(file);
    return configs;
}

static void delete_configs(char ***configs){
    int i = 0;
    while(configs[i] != NULL){
        free(configs[i][0]);
        free(configs[i][1]);
        free(configs[i]);
        i++;
    }
}

static char * get_config_s(char ***configs, const char *name){
    int i = 0;
    while(configs[i] != NULL){
        if(strcmp(configs[i][0], name) == 0)
            return configs[i][1];
        i++;
    }
    return NULL;
}

static int get_config_d(char ***configs, const char *name){
    char * s = get_config_s(configs, name);
    if(s == NULL)
        return -1;
    return strtol(s, NULL, 10);
}

// Checking output from different parallelism just to be sure that everything is ok
void debugoutput(){
    int fd;
    char namefile[128];
    snprintf(namefile, 128, "%s_%dx%d_%d_%ld.data", "data_raw", width, height, max_iter, time(NULL));
    if((fd = open(namefile, O_CREAT|O_WRONLY)) != -1){
        printf("[+] Created %s with raw data! Check them with hash function\n", namefile);
        for(int i=0; i < (width*height); i++)
            write(fd, &(iterations[i]), sizeof(int));
        close(fd);
    }
}

void set_rgbdouble(rgbdouble_t *rgb_d, unsigned int color){
    rgb_d->r = ((double)((color & 0x00ff0000) >> 16))/255;
    rgb_d->g = ((double)((color & 0x0000ff00) >> 8))/255;
    rgb_d->b = ((double)( color & 0x000000ff) )/255;
}


#ifndef NO_GTK
#include <gtk/gtk.h>
static cairo_surface_t *mysurface = NULL;

void destroy(GtkWidget *widget, gpointer data){
    gtk_main_quit();
}

/* Redraw the screen from the surface. Note that the draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, gpointer Sdata){
    cairo_set_source_surface(cr, mysurface, 0, 0);
    cairo_paint(cr);
    return FALSE;
}

/* n grandi n/max_iter --> 1  (colore più intenso)
 * RED --> R:255 G:0 B:0
 * low n (iterazioni) sono i punti più lontani, quelli che divergono subito
 */
static __inline__ gboolean update_view_internal(GtkWidget *widget, GdkEventKey *event){
    rgbdouble_t c, f;
    unsigned int color;
    unsigned int fullscale = COLOR_RGB;     // only blue change in => 0x000000ff;
    set_rgbdouble(&f, fullscale);
    cairo_t *cr = cairo_create(mysurface);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, 1);
    for(int i = 0; i < height; i++){
        for(int j = 0; j < width; j++){
            if(iterations[getid(i,j,width)] == -1){     //not in mandelbrot set
                cairo_set_source_rgb(cr, BG_R, BG_G, BG_B);
            }
            else{
                color = (fullscale * ((unsigned int)iterations[getid(i,j,width)])) / max_iter;
                set_rgbdouble(&c, color);
                cairo_set_source_rgb(cr, (f.r-c.r), (f.g-c.g), (f.b-c.b));  // OLD (cr, 1-r, 1-g, 1-b)
            }
            cairo_rectangle(cr, j, i, 1, 1);
            cairo_fill(cr);
        }
    }
    cairo_destroy(cr);
    gtk_widget_queue_draw(widget);
    return TRUE;
}

static gboolean update_view(GtkWidget *widget, GdkEventKey *event){
    clocktype_t t;
    char *action_s;
    char action = 'x';
    if(event){
        action_s = gdk_keyval_name(event->keyval);
        if(strcmp(action_s, "Return") == 0) action = 'Z';
        else action = action_s[0];
    }
    if(mysurface == NULL || zoom_adjust_c(action) == -1)
        return FALSE;
    clock_time_start();
    core_computation();
    t = clock_time_stop();
    time_sum = clock_time_add(time_sum, t);
    printf("[%c] ", action);
    print_time_detailed(t, "elaboration time   ");
    return update_view_internal(widget, event);
}

int graphic_main(int argc, char *argv[], void **args){
    GtkWidget *window;
    GtkWidget *drawarea;
    gtk_init(NULL, NULL);   //gtk_init(&argc, &argv);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL); //create a new window
    drawarea = gtk_drawing_area_new();
    //gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);
    gtk_widget_set_size_request(drawarea, width, height);
    gtk_container_add(GTK_CONTAINER(window), drawarea);
    gtk_container_set_border_width(GTK_CONTAINER (window), 0);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_widget_show(drawarea);
    gtk_widget_show_all(window);
    // ok
    g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
    g_signal_connect(drawarea, "draw", G_CALLBACK(draw_cb), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(update_view), NULL);
    gtk_widget_add_events(drawarea, GDK_POINTER_MOTION_MASK);
    mysurface = gdk_window_create_similar_surface(gtk_widget_get_window (drawarea), CAIRO_CONTENT_COLOR,
                gtk_widget_get_allocated_width(drawarea), gtk_widget_get_allocated_height(drawarea));
    if(argv) update_view(window, NULL); //clear_surface();
    else update_view_internal(window, NULL);
    gtk_main();
    return 0;
}
#endif

int textual_main(char *sequence, void **args){
    int i = 0;
    clocktype_t t;
    if(sequence == NULL){
        printf("[-] Error sequence!\n");
        return -1;
    }
    while(sequence[i] != '\0'){
        if(zoom_adjust_c(sequence[i]) != -1){
            clock_time_start();
            core_computation();
            t = clock_time_stop();
            time_sum = clock_time_add(time_sum, t);
            printf("[%c] ", sequence[i]);
            print_time_detailed(t, "elaboration time   ");
        }
        i++;
    }
    return 0;
}

void usage(){
    char usage[] = "Mandelbrot and Julia set parallel computing\n\
    -t      textual mode, with -draw the window is opened only at the end\n\
    -n      number of threads / processes \n\
    -g      grain of the algorithm \n\
    -julia  calculate julia set instead mandelbrot set \n\
    -iter   overwrite the maximum number of iterations defined in config file \n\
    \n\
    example:    ./mandelxxx -iter 500 \n\
    ";
    printf("%s\n", usage);
    exit(0);
}

void** get_args_from_argv(int argc, char *argv[]){
    void **args = (void**) malloc(sizeof(void*) * 16);
    memset(args, 0, sizeof(void*) * 16);
    args[ARG_ARGC] = (void*)(long)argc;
    args[ARG_ARGV] = (void*)argv;
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--help") == 0) usage();
        else if(strcmp(argv[i], "-t") == 0) args[ARG_TEXT] = (void*)1;
        else if(strcmp(argv[i], "-julia") == 0) args[ARG_JULY] = argv[i];
        else if(strcmp(argv[i], "-draw") == 0) args[ARG_DRAW] = argv[i];
        else if(strcmp(argv[i], "-n") == 0) args[ARG_NUMP]  = argv[i+1];
        else if(strcmp(argv[i], "-g") == 0) args[ARG_GRAIN] = argv[i+1];
        else if(strcmp(argv[i], "-iter") == 0) args[ARG_ITER] = argv[i+1];
        else if(strncmp(argv[i], "-r=", 3) == 0) args[ARG_JULR] = &argv[i][3];
        else if(strncmp(argv[i], "-i=", 3) == 0) args[ARG_JULI] = &argv[i][3];

    }
    return args;
}

int main(int argc, char *argv[]){
    char ***configs = read_configs();               //exit on errors
    void **args = get_args_from_argv(argc, argv);   //exit with --help
    char msg[100];
    if(args[ARG_ITER]) max_iter = strtol((char*)args[ARG_ITER], NULL, 10);
    else max_iter = get_config_d(configs, "MAX_ITER");
    zoom_info.move_px = get_config_d(configs, "MOVE_PIXEL");
    width = get_config_d(configs, "SIZE_WIDTH");
    height = get_config_d(configs, "SIZE_HEIGHT");
    matrix = (complex_t*) malloc(sizeof(complex_t) * (width*height));
    iterations = (int*) malloc(sizeof(int) * (width*height));
    core_init(args, msg);
    printf("[+] #########################################################\n");
    printf("[+]   %s %s mode\n", (args[ARG_JULY])?"Julia":"Mandelbrot", (args[ARG_TEXT])?"Textual":"Graphic");
    printf("[+]   Matrix %d x %d = %d points\n", width, height, width*height);
    printf("[+]   Maximum %d iterations per pixel\n", max_iter);
    printf("[+]   %s\n", msg);
    printf("[+] - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
    if(args[ARG_TEXT]){
        textual_main(get_config_s(configs, "SEQUENCE"), args);
        #ifndef NO_GTK
        if(args[ARG_DRAW])  // paint_results (open gtk window at the end)
            graphic_main(0, NULL, args);
        #endif
    }
    else{
        #ifndef NO_GTK
        graphic_main(argc, argv, args);
        #else
        textual_main(get_config_s(configs, "SEQUENCE"), args);
        #endif
    }
    core_exit(NULL);
    //debugoutput();
    print_time(time_sum, "[+] TOTAL WORK TIME  ==>  ");
    print_time_detailed(time_sum, "[+] TOTAL WORK TIME  ==>  ");
    delete_configs(configs);
    free(args);
    free(iterations);
    free(matrix);
}
