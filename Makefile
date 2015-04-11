CFLAGS = -g -std=c99 -Wall -pedantic -Isrc

PLATFORM = stb_image.o stb_truetype.o fontstash.o image.o
LIBRARIES = -lm -lOpenCL
SDIR = src

OBJS = $(PLATFORM) main.o mesh.o 3dmaths.o gui.o text.o  shader.o \
	http.o ocl.o


# Build rules
WDIR = build/win
_WOBJS = $(OBJS) gpuinfo.o GL/glew.o win32.o win32.res
WOBJS = $(patsubst %,$(WDIR)/%,$(_WOBJS))
WLIBS = $(LIBRARIES) -lgdi32 -lopengl32 -lwinmm -lws2_32 -lOpenCL
#	-L"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v7.0\lib\x64"
#	-L"C:\Program Files (x86)\AMD APP SDK\3.0-0-Beta\lib\x86_64"

LDIR = build/lin
LCC = gcc
_LOBJS = $(OBJS) gpuinfo.o GL/glew.o x11.o
LOBJS = $(patsubst %,$(LDIR)/%,$(_LOBJS))
LLIBS = $(LIBRARIES) -lGL -lX11 -lGLU -lXi -ldl

MDIR = build/mac
MCC = clang
_MOBJS = $(OBJS)
MFLAGS = -Wall
MOBJS = $(patsubst %,$(MDIR)/%,$(_MOBJS))
MLIBS = -F/System/Library/Frameworks -framework OpenGL -framework CoreVideo -framework Cocoa -framework OpenCL



# Evil platform detection magic
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
default: gui
WCC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres
#WCC = i686-w64-mingw32-gcc
#WINDRES = i686-w64-mingw32-windres
endif
ifeq ($(UNAME), MINGW32_NT-6.1)
WCC = gcc
WINDRES = windres
default: gui.exe
endif
ifeq ($(UNAME), Darwin)
default: gui.app
endif 


$(WDIR)/Icon.ico: $(SDIR)/Icon.png
	convert -resize 256x256 $^ $@
$(WDIR)/win32.res: $(SDIR)/win32.rc $(WDIR)/Icon.ico
	$(WINDRES) -I $(WDIR) -O coff src/win32.rc -o $@
$(WDIR)/%.o: $(SDIR)/%.c
	$(WCC) $(CFLAGS) -DWIN32 $(INCLUDES)-c $< -o $@
gui.exe: $(WOBJS)
	$(WCC) $^ $(WLIBS) -o $@

$(LDIR)/icon.rgba: $(SDIR)/Icon.png
	convert -resize 48x48 $^ $@
$(LDIR)/icon.h: $(LDIR)/icon.rgba
	bin2h 16 < $^ > $@
$(LDIR)/x11.o: $(SDIR)/x11.c $(LDIR)/icon.h
	$(LCC) $(CFLAGS) $(INCLUDES) -I$(LDIR) -c $< -o $@
$(LDIR)/%.o: $(SDIR)/%.c
	$(LCC) $(CFLAGS) $(INCLUDES) -c $< -o $@
gui: $(LOBJS)
	$(LCC) $^ $(LLIBS) -o $@


# generate the Apple Icon file from src/Icon.png
$(MDIR)/AppIcon.iconset/icon_512x512@2x.png: src/Icon.png
	cp $^ $@
$(MDIR)/AppIcon.iconset/icon_512x512.png: src/Icon.png
	cp $^ $@
	sips -Z 512 $@
$(MDIR)/AppIcon.icns: $(MDIR)/AppIcon.iconset/icon_512x512@2x.png $(MDIR)/AppIcon.iconset/icon_512x512.png
	iconutil -c icns $(MDIR)/AppIcon.iconset
	iconutil -c iconset $@
	iconutil -c icns $(MDIR)/AppIcon.iconset
# build the Apple binary
$(MDIR)/osx.o: $(SDIR)/osx.m
	$(MCC) $(MFLAGS) -c $< -o $@
$(MDIR)/%.o: $(SDIR)/%.c
	$(MCC) $(CFLAGS) $(INCLUDES)-c $< -o $@
gui.bin: $(MOBJS) $(MDIR)/osx.o
	$(MCC) $^ $(MLIBS) -o $@
# generate the Apple .app file
gui.app/Contents/_CodeSignature/CodeResources: gui.bin src/Info.plist $(MDIR)/AppIcon.icns
	rm -rf gui.app
	mkdir -p gui.app/Contents/MacOS
	mkdir gui.app/Contents/Resources
	cp gui.bin gui.app/Contents/MacOS/gui
	cp src/Info.plist gui.app/Contents
	cp $(MDIR)/AppIcon.icns gui.app/Contents/Resources
	codesign --force --sign - gui.app
gui.app: gui.app/Contents/_CodeSignature/CodeResources

# build a zip of the windows exe
voxel.zip: gui.exe
	zip voxel.zip gui.exe README.md LICENSE data/shaders/* data/gui/* data/stanford-bunny.obj

# Housekeeping
clean:
	@rm -rf build gui gui.exe gui.bin gui.app voxel.zip src/version.h

# for QtCreator
all: default

# Create build directories
$(shell	mkdir -p build/lin/GL build/win/GL build/mac/AppIcon.iconset)

# create the version info
$(shell echo "#define GIT_REV \"`git rev-parse --short HEAD`\"" > src/version.h)
$(shell echo "#define GIT_TAG \"`git name-rev --tags --name-only \`git rev-parse HEAD\``\"" >> src/version.h)

