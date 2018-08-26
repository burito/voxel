define GET_HELP

Please read the instructions at...
	http://danpburke.blogspot.com.au/2017/06/fresh-system-install.html
...for help
endef

CFLAGS = -std=c11 -Ilib/include
VPATH = src lib

OBJS = stb_image.o stb_truetype.o fontstash.o fast_atof.o image.o main.o mesh.o 3dmaths.o gui.o text.o  shader.o \
	http.o ocl.o clvoxel.o gpuinfo.o
DEBUG = -g
#DEBUG =

# Build rules
WDIR = build/win
_WOBJS = $(OBJS) glew.o win32.o win32.res
WOBJS = $(patsubst %,$(WDIR)/%,$(_WOBJS))
WINLIBS = -lshell32 -luser32 -lgdi32 -lopengl32 -lwinmm -lws2_32 -lxinput9_1_0 `which opencl.dll`


LDIR = build/lin
LCC = clang
_LOBJS = $(OBJS) glew.o x11.o 
LOBJS = $(patsubst %,$(LDIR)/%,$(_LOBJS))
LLIBS = -lm -lGL -lX11 -lGLU -lXi -ldl -lOpenCL

MDIR = build/mac
MCC = clang
_MOBJS = $(OBJS) osx.o
MFLAGS = -Wall
MOBJS = $(patsubst %,$(MDIR)/%,$(_MOBJS))
MLIBS = -F/System/Library/Frameworks -F. -framework OpenGL -framework CoreVideo -framework Cocoa -framework IOKit -framework OpenCL -framework ForceFeedback


# Evil platform detection magic
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)	# if Apple
default: gui.app
else
# Windows & Linux need ImageMagick, lets check for it
ifeq (magick,$(findstring magick, $(shell which magick 2>&1))) # current ImageMagick looks like this
MAGICK = magick convert
else
	ifeq (convert,$(findstring convert, $(shell which convert 2>&1))) # Ubuntu ships a very old ImageMagick that looks like this
MAGICK = convert
	else
$(error Can't find ImageMagick installation $(GET_HELP))	
	endif
endif # ImageMagick check done!

ifeq ($(UNAME), Linux)	# if Linux
default: gui
# this entry is for building windows binaries on linux with mingw64
WCC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres
WLIBS = $(LOCAL_DLL) $(WINLIBS)
#WCC = i686-w64-mingw32-gcc
#WINDRES = i686-w64-mingw32-windres
else	# if Windows

ifdef WCC	# allow user to override compiler choice
$(info "$(WCC)")
	ifeq (clang,$(WCC))
$(info this was called)
WCC := clang -target x86_64-pc-windows-gnu
	else
	WCC = $(WCC)
	endif
	WLIBS = $(LOCAL_DLL) $(WINLIBS)
else
	ifdef VS120COMNTOOLS # we have MSVC 2017 installed
		WLIBS = $(LOCAL_LIB) $(WINLIBS)
		WCC = clang -target x86_64-pc-windows-gnu
	else
		WLIBS = $(LOCAL_DLL) $(WINLIBS)
		ifeq (,$(findstring which, $(shell which clang 2>&1))) # clang present?
			WCC = clang -target x86_64-pc-windows-gnu
		else
			ifeq (,$(findstring which, $(shell which gcc 2>&1))) # gcc present?
				WCC = gcc
			else
$(error Can't find a compiler. $(GET_HELP))
			endif
		endif
	endif
endif
# end compiler detection
WINDRES = windres
default: gui.exe
endif
endif


$(WDIR)/Icon.ico: Icon.png
	$(MAGICK) -resize 256x256 $^ $@
$(WDIR)/win32.res: win32.rc $(WDIR)/Icon.ico
	$(WINDRES) -I $(WDIR) -O coff src/win32.rc -o $@
$(WDIR)/%.o: %.c
	$(WCC) $(DEBUG) $(CFLAGS) $(INCLUDES)-c $< -o $@
gui.exe: $(WOBJS)
	$(WCC) $(DEBUG) $(WOBJS) $(WLIBS) -o $@

# crazy stuff to get icons on x11
$(LDIR)/x11icon: x11icon.c
	$(LCC) $^ -o $@
$(LDIR)/icon.rgba: Icon.png
	$(MAGICK) -resize 256x256 $^ $@
#	magick convert -resize 256x256 $^ $@
$(LDIR)/icon.argb: $(LDIR)/icon.rgba $(LDIR)/x11icon
	./build/lin/x11icon < $(LDIR)/icon.rgba > $@
$(LDIR)/icon.h: $(LDIR)/icon.argb
	bin2h 13 < $^ > $@
$(LDIR)/x11.o: x11.c $(LDIR)/icon.h
	$(LCC) $(CFLAGS) $(INCLUDES) -I$(LDIR) -c $< -o $@
$(LDIR)/%.o: %.c
	$(LCC) $(DEBUG) $(CFLAGS) $(INCLUDES) -c $< -o $@
gui: $(LOBJS)
	$(LCC) $(DEBUG) $^ $(LLIBS) -o $@


# generate the Apple Icon file from src/Icon.png
$(MDIR)/AppIcon.iconset/icon_512x512@2x.png: Icon.png
	cp $^ $@
$(MDIR)/AppIcon.iconset/icon_512x512.png: Icon.png
	cp $^ $@
	sips -Z 512 $@
$(MDIR)/AppIcon.icns: $(MDIR)/AppIcon.iconset/icon_512x512@2x.png $(MDIR)/AppIcon.iconset/icon_512x512.png
	iconutil -c icns $(MDIR)/AppIcon.iconset
# build the Apple binary
$(MDIR)/%.o: %.m
	$(MCC) $(MFLAGS) -c $< -o $@
$(MDIR)/%.o: %.c
	$(MCC) $(DEBUG) $(CFLAGS) $(INCLUDES)-c $< -o $@
gui.bin: $(MOBJS) $(MDIR)/osx.o
# the library has to have it's path before the executable is linked
	install_name_tool -id @executable_path/../Frameworks/OpenVR.framework/Versions/A/OpenVR lib/mac/OpenVR
	$(MCC) $(DEBUG) $^ $(MLIBS) -rpath @loader_path/ -o $@
#	$(MCC) $^ $(MLIBS) -rpath @loader_path/../Frameworks -o $@
# generate the Apple .app file
gui.app/Contents/_CodeSignature/CodeResources: gui.bin src/Info.plist $(MDIR)/AppIcon.icns
	rm -rf gui.app
	mkdir -p gui.app/Contents
	cp src/Info.plist gui.app/Contents
	mkdir gui.app/Contents/MacOS
	cp gui.bin gui.app/Contents/MacOS/gui
	mkdir gui.app/Contents/Resources
	cp $(MDIR)/AppIcon.icns gui.app/Contents/Resources
	codesign --force --deep --sign - gui.app
gui.app: gui.app/Contents/_CodeSignature/CodeResources

# build a zip of the windows exe
voxel.zip: gui.exe
	zip opengl.zip gui.exe README.md LICENSE

# Housekeeping
clean:
	@rm -rf build gui gui.exe gui.bin gui.app opengl.zip src/version.h gui.pdb gui.ilk

help:
	echo Possible targets are...
	echo 	make 		# build the default target for the current platform
	echo 	make clean	# remove intermediate build files
	echo 	make help	# display this message

# for QtCreator
all: default

# Create build directories
$(shell	mkdir -p build/lin/GL build/win/GL build/mac/AppIcon.iconset)

# create the version info
$(shell echo "#define GIT_REV \"`git rev-parse --short HEAD`\"" > src/version.h)
$(shell echo "#define GIT_TAG \"`git name-rev --tags --name-only \`git rev-parse HEAD\``\"" >> src/version.h)

