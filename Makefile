CFLAGS = -std=c99 -Wall -pedantic -g
CC = gcc

# Evil platform detection magic
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LIBRARIES = -lm -lGL -lX11 -lGLU -lz -lXi
PLATFORM = x11
endif
ifeq ($(UNAME), MINGW32_NT-6.1)
PLATFORM = glew.o win32.res win32
CFLAGS += -DWIN32 -DGLEW_STATIC
LIBRARIES = -lgdi32 -lopengl32 -lz
# LIBRARIES += -mwindows
endif


# Build rules
default: convert polyview octview convertoct

convertoct: convertoct.o 3dmaths.o
	$(CC) $^ $(LIBRARIES) -o $@

convert: convert.o 3dmaths.o
	$(CC) $^ $(LIBRARIES) -o $@ 

polyview: polyview.o $(PLATFORM).o
	$(CC) $^ $(LIBRARIES) -o $@

octview: octview.o shader.o voxel.o $(PLATFORM).o
	$(CC) $^ $(LIBRARIES) -o $@

win32.res: win32.rc
	windres $^ -O coff -o $@

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES)-c $< -o $@

# Testing rules
data: convert 
	./convert data/stanford-bunny.obj data/stanford-bunny.msh
#	./convert data/xyzrgb-dragon.obj data/xyzrgb-dragon.msh

test: octview convertoct
#	./octview data/stanford-bunny.oct
	./octview data/xyzrgb-dragon.oct

testpoly: polyview
	./polyview data/stanford-bunny.msh
#	./polyview data/stanford-bunny.msh

testoct: convertoct
#	./convertoct data/stanford-bunny.msh data/stanford-bunny.oct
	./convertoct data/xyzrgb-dragon.msh data/xyzrgb-dragon.oct

# Housekeeping
copytowin:
	bash -c cp {*.{c,h},Makefile,data/render.*} /media/wd/code/voxel

copyfromwin:
	cp /media/wd/code/voxel/{*.{c,h},Makefile,data/render.*} ~/code/voxel

clean:
	rm -f *.o convert convertoct polyview octview *.exe win32.res

