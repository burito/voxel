CFLAGS = -g -std=c99 -Wall -pedantic -I.
CC = gcc
PLATFORM = GL/glew.o stb_image.o stb_truetype.o fontstash.o image.o
LIBRARIES = -lm -lOpenCL
# Evil platform detection magic
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LIBRARIES += -lGL -lX11 -lGLU -lXi
PLATFORM += x11.o
endif
ifeq ($(UNAME), MINGW32_NT-6.1)
PLATFORM += win32.res win32.o
CFLAGS += -DWIN32 
LIBRARIES += -lgdi32 -lopengl32 -lwinmm
endif

# Build rules
default: gui


win32.res: win32.rc
	windres $^ -O coff -o $@

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES)-c $< -o $@


meshtest.o: mesh.c mesh.h
	$(CC) -DSTATIC_TEST $(CFLAGS) $(INCLUDES) -c mesh.c -o $@

meshtest: meshtest.o text.o 3dmaths.o image.o stb_image.o
	$(CC) $^ $(LIBRARIES) -o $@


gui: main.o mesh.o 3dmaths.o gui.o text.o ocl.o $(PLATFORM)
	$(CC) $^ $(LIBRARIES) -o $@


# Testing rules
data: convertoct
	./convertoct data/stanford-bunny.msh data/stanford-bunny.oct
	./convertoct data/xyzrgb-dragon.msh data/xyzrgb-dragon.oct

test: octview convertoct
#	./octview data/stanford-bunny.oct
	./octview data/xyzrgb-dragon.oct

# Housekeeping
clean:
	@rm -f *.o GL/glew.o convertoct octview gui *.exe win32.res

