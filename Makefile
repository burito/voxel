# *** Platform detection
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
# MacOS
BUILD_DIR = $(MAC_DIR)
CC = clang -g
default: voxel.app

else
# Windows & Linux need ImageMagick, lets check for it
ifeq (magick,$(findstring magick, $(shell which magick 2>&1))) # current ImageMagick looks like this
MAGICK = magick convert
else
	ifeq (convert,$(findstring convert, $(shell which convert 2>&1))) # Ubuntu ships a very old ImageMagick that looks like this
MAGICK = convert
	else
$(error Can't find ImageMagick installation, try...)
		ifeq ($(UNAME), Linux)
			$(error		apt-get install imagemagick)
		else
			$(error		https://www.imagemagick.org/script/download.php)
		endif
	endif
endif # ImageMagick check done!

ifeq ($(UNAME), Linux)
# Linux
BUILD_DIR = $(LIN_DIR)
CC = clang -g
default: voxel

else
# Windows
BUILD_DIR = $(WIN_DIR)
WINDRES = windres
CC = gcc -g
default: voxel.exe
endif
endif

OBJS = version.o log.o global.o stb_image.o stb_truetype.o fontstash.o fast_atof.o image.o main.o mesh.o 3dmaths.o gui.o text.o shader.o http.o ocl.o clvoxel.o gpuinfo.o
CFLAGS = -std=c11 -Ideps/include -Ideps/dpb/src
VPATH = src build deps deps/dpb/src

WIN_LIBS = `which opencl.dll` -luser32 -lwinmm -lgdi32 -lshell32 -lopengl32 -lws2_32 -lxinput9_1_0
LIN_LIBS = -lm -lGL -lX11 -lGLU -lXi -ldl -lOpenCL
MAC_LIBS = -F/System/Library/Frameworks -F. -framework OpenGL -framework CoreVideo -framework Cocoa -framework IOKit -framework OpenCL -framework ForceFeedback

WIN_DIR = build/win
LIN_DIR = build/lin
MAC_DIR = build/mac

_WIN_OBJS = $(OBJS) glew.o win32.o win32.res
_LIN_OBJS = $(OBJS) glew.o x11.o 
_MAC_OBJS = $(OBJS) osx.o

WIN_OBJS = $(patsubst %,$(WIN_DIR)/%,$(_WIN_OBJS))
LIN_OBJS = $(patsubst %,$(LIN_DIR)/%,$(_LIN_OBJS))
MAC_OBJS = $(patsubst %,$(MAC_DIR)/%,$(_MAC_OBJS))

MAC_BUNDLE = voxel

$(WIN_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIN_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(MAC_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(MAC_DIR)/%.o: %.m
	$(CC) $(CFLAGS) -c $< -o $@

voxel.exe: $(WIN_OBJS)
	$(CC) $^ $(WIN_LIBS) -o $@

voxel: $(LIN_OBJS)
	$(CC) $^ $(LIN_LIBS) -o $@

voxel.bin: $(MAC_OBJS)
	$(CC) $(CFLAGS) $^ $(MAC_LIBS) -o $@

# start build the win32 Resource File (contains the icon)
$(WIN_DIR)/Icon.ico: Icon.png
	$(MAGICK) -resize 256x256 $< $@
$(WIN_DIR)/win32.res: win32.rc $(WIN_DIR)/Icon.ico
	$(WINDRES) -I $(WIN_DIR) -O coff $< -o $@
# end build the win32 Resource File

# start build the App Bundle (apple)
# generate the Apple Icon file from src/Icon.png
$(MAC_DIR)/AppIcon.iconset:
	mkdir $@
$(MAC_DIR)/AppIcon.iconset/icon_512x512@2x.png: Icon.png $(MAC_DIR)/AppIcon.iconset
	cp $< $@
$(MAC_DIR)/AppIcon.iconset/icon_512x512.png: Icon.png $(MAC_DIR)/AppIcon.iconset
	sips -Z 512 $< --out $@ 1>/dev/null
$(MAC_DIR)/AppIcon.icns: $(MAC_DIR)/AppIcon.iconset/icon_512x512@2x.png $(MAC_DIR)/AppIcon.iconset/icon_512x512.png
	iconutil -c icns $(MAC_DIR)/AppIcon.iconset

MAC_CONTENTS = $(MAC_BUNDLE).app/Contents

.PHONY: $(MAC_BUNDLE).app
$(MAC_BUNDLE).app : $(MAC_CONTENTS)/_CodeSignature/CodeResources

# this has to list everything inside the app bundle
$(MAC_CONTENTS)/_CodeSignature/CodeResources : \
	$(MAC_CONTENTS)/MacOS/$(MAC_BUNDLE) \
	$(MAC_CONTENTS)/Resources/AppIcon.icns \
	$(MAC_CONTENTS)/Info.plist
	codesign --force --deep --sign - $(MAC_BUNDLE).app

$(MAC_CONTENTS)/Info.plist: src/Info.plist
	@mkdir -p $(MAC_CONTENTS)
	cp $< $@

$(MAC_CONTENTS)/Resources/AppIcon.icns: $(MAC_DIR)/AppIcon.icns
	@mkdir -p $(MAC_CONTENTS)/Resources
	cp $< $@

# copies the binary, and tells it where to find libraries
$(MAC_CONTENTS)/MacOS/$(MAC_BUNDLE): $(MAC_BUNDLE).bin
	@mkdir -p $(MAC_CONTENTS)/MacOS
	cp $< $@

.DELETE_ON_ERROR :
# end build the App Bundle

# crazy stuff to get icons on x11
$(LIN_DIR)/x11icon: x11icon.c
	$(LCC) $^ -o $@
$(LIN_DIR)/icon.rgba: Icon.png
	$(MAGICK) -resize 256x256 $^ $@
#	magick convert -resize 256x256 $^ $@
$(LIN_DIR)/icon.argb: $(LIN_DIR)/icon.rgba $(LIN_DIR)/x11icon
	./build/lin/x11icon < $(LIN_DIR)/icon.rgba > $@
$(LIN_DIR)/icon.h: $(LIN_DIR)/icon.argb
	bin2h 13 < $^ > $@
$(LIN_DIR)/x11.o: x11.c $(LIN_DIR)/icon.h
	$(LCC) $(CFLAGS) $(INCLUDES) -I$(LIN_DIR) -c $< -o $@
$(LIN_DIR)/%.o: %.c
	$(LCC) $(DEBUG) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Housekeeping
clean:
	@rm -rf build gui gui.exe gui.bin gui.app opengl.zip src/version.c gui.pdb gui.ilk

help:
	echo Possible targets are...
	echo 	make 		# build the default target for the current platform
	echo 	make clean	# remove intermediate build files
	echo 	make help	# display this message


# Create build directories
$(shell mkdir -p $(BUILD_DIR))
# $(shell	mkdir -p build/lin/GL build/win/GL build/mac/AppIcon.iconset)

# create the version info
GIT_VERSION:=$(shell git describe --dirty --always --tags)
VERSION:=const char git_version[] = "$(GIT_VERSION)";
SRC_VERSION:=$(shell cat build/version.c 2>/dev/null)
ifneq ($(SRC_VERSION),$(VERSION))
$(shell echo '$(VERSION)' > build/version.c)
endif
