CFLAGS = -g -std=c99 -Wall -pedantic -Isrc
PLATFORM = GL/glew.o stb_image.o stb_truetype.o fontstash.o image.o gpuinfo.o
LIBRARIES = -lm
SDIR = src

OBJS = $(PLATFORM) main.o mesh.o 3dmaths.o gui.o text.o clvoxel.o shader.o \
	http.o

# Build rules


WDIR = build/win
#WCC = i686-w64-mingw32-gcc
#WINDRES = i686-w64-mingw32-windres
WCC = wine64 ~/mingw64/bin/gcc.exe
WINDRES = wine64 ~/mingw64/bin/windres.exe
#WCC = x86_64-w64-mingw32-gcc
#WINDRES = x86_64-w64-mingw32-windres
_WOBJS = $(OBJS) win32.o win32.res
WOBJS = $(patsubst %,$(WDIR)/%,$(_WOBJS))
WLIBS = $(LIBRARIES) -lgdi32 -lopengl32 -lwinmm -lws2_32


LDIR = build/lin
LCC = gcc
_LOBJS = $(OBJS) x11.o
LOBJS = $(patsubst %,$(LDIR)/%,$(_LOBJS))
LLIBS = $(LIBRARIES) -lGL -lX11 -lGLU -lXi -ldl

# Evil platform detection magic
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
default: gui

endif
ifeq ($(UNAME), MINGW32_NT-6.1)
WINDRES = windres
default: gui.exe
endif

$(WDIR)/win32.res: $(SDIR)/win32.rc
	$(WINDRES) $^ -O coff -o $@

$(WDIR)/%.o: $(SDIR)/%.c
	$(WCC) $(CFLAGS) -DWIN32 $(INCLUDES)-c $< -o $@
$(LDIR)/%.o: $(SDIR)/%.c
	$(LCC) $(CFLAGS) $(INCLUDES)-c $< -o $@


all: default    # for QtCreator

gui: $(LOBJS)
	$(LCC) $^ $(LLIBS) -o $@

gui.exe: $(WOBJS)
	$(WCC) $^ $(WLIBS) -o $@


voxel.zip: gui.exe
	zip voxel.zip gui.exe README LICENSE data/shaders/* data/gui/* data/stanford-bunny.obj



# Testing rules
data: convertoct
	./convertoct data/stanford-bunny.msh data/stanford-bunny.oct
	./convertoct data/xyzrgb-dragon.msh data/xyzrgb-dragon.oct

test: octview convertoct
#	./octview data/stanford-bunny.oct
	./octview data/xyzrgb-dragon.oct

# Housekeeping
clean:
	@rm -rf build gui gui.exe voxel.zip src/version.h


# Create build directories
$(shell	mkdir -p build/lin/GL build/win/GL)

# create the version info
$(shell echo "#define GIT_REV \"`git rev-parse --short HEAD`\"" > src/version.h)
$(shell echo "#define GIT_TAG \"`git name-rev --tags --name-only \`git rev-parse HEAD\``\"" >> src/version.h)

