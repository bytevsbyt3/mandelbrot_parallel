CC=gcc
CCNV=gcc
CCOLD=gcc
GTKFLAGS=`pkg-config --cflags --libs gtk+-3.0`
GLO=common
GPU=gpu
PTH=pthread
MPI=mpi
SEQ=sequential
CORE=mandelbrot_core
USEGTK=YES

ifeq ($(USEGTK), YES)
	GTKLINKFLAG=-lm $(GTKFLAGS)
	GTKCONSTDEF=
else
	GTKLINKFLAG=
	GTKCONSTDEF=-D NO_GTK
endif

mandelbrot_gui: $(GLO)/mandelbrot_gui.cpp $(GLO)/mandelbrot.h
	$(CC) -c -Wall -pedantic $(GTKLINKFLAG) $(GTKCONSTDEF) -o $(GLO)/mandelbrot_gui.o $(GLO)/mandelbrot_gui.cpp

mandelgpu: mandelbrot_gui $(GPU)/$(CORE).cu
	nvcc -c $(GPU)/$(CORE).cu -o $(GPU)/$(CORE).o -ccbin $(CCNV) -Wno-deprecated-gpu-targets
	g++ $(GPU)/$(CORE).o $(GLO)/mandelbrot_gui.o $(GLO)/time_util.cpp -L/usr/local/cuda/lib64 -lcudart -lcuda $(GTKLINKFLAG) -o mandelgpu

mandelpth: mandelbrot_gui $(PTH)/$(CORE).cpp
	$(CC) -pthread $(GLO)/mandelbrot_gui.o $(PTH)/$(CORE).cpp $(GLO)/time_util.cpp -Wall -pedantic $(GTKLINKFLAG) -o mandelpth

mandelmpi: mandelbrot_gui $(MPI)/$(CORE).cpp
	mpic++ $(MPI)/$(CORE).cpp $(GLO)/mandelbrot_gui.o $(GLO)/time_util.cpp -Wall -pedantic $(GTKLINKFLAG) -o mandelmpi

mandelseq: mandelbrot_gui $(SEQ)/$(CORE).cpp
	$(CC) $(GLO)/mandelbrot_gui.o $(GLO)/time_util.cpp $(SEQ)/$(CORE).cpp -Wall -pedantic $(GTKLINKFLAG) -o mandelseq

all: mandelpth mandelseq mandelgpu mandelmpi

.PHONY: clean
clean:
	rm -f mandelgpu mandelpth mandelmpi mandelseq mandelgpu_cu8 *.o common/*.o $(GPU)/*.o
