CFLAGS = -g -std=c99 -Wall -pedantic
CFLAGS += -I.
CC = gcc
PLATFORM = GL/glew.o
LIBRARIES = -lm -lz
# Evil platform detection magic
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LIBRARIES += -lGL -lX11 -lGLU -lXi
PLATFORM += x11.o
endif
ifeq ($(UNAME), MINGW32_NT-6.1)
PLATFORM += win32.res win32.o
CFLAGS += -DWIN32 
LIBRARIES += -lgdi32 -lopengl32
endif

# Build rules
default: octview convertoct gui


win32.res: win32.rc
	windres $^ -O coff -o $@

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES)-c $< -o $@


convertoct: convertoct.o 3dmaths.o
	$(CC) $^ $(LIBRARIES) -o $@

octview: octview.o shader.o voxel.o $(PLATFORM)
	$(CC) $^ $(LIBRARIES) -o $@

gui: main.o mesh.o 3dmaths.o gui.o fontstash.o text.o $(PLATFORM)
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
	@rm -f *.o GL/glew.o convert convertoct polyview octview gui *.exe win32.res

