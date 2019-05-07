
COMPANY = Daniel Burke
COPYRIGHT = 2013-2019
DESCRIPTION = Voxel Prototype
BINARY_NAME = voxel
OBJS = version.o log.o global.o stb_image.o stb_truetype.o fontstash.o fast_atof.o image.o main.o mesh.o 3dmaths.o gui.o text.o shader.o http.o ocl.o clvoxel.o gpuinfo.o
CFLAGS = -std=c11 -Ideps/include -Ideps/dpb/src
VPATH = src build deps deps/dpb/src

WIN_LIBS = `which opencl.dll` -luser32 -lwinmm -lgdi32 -lshell32 -lopengl32 -lws2_32 -lxinput9_1_0
LIN_LIBS = -lm -lGL -lX11 -lGLU -lXi -ldl -lOpenCL
MAC_LIBS = -F/System/Library/Frameworks -F. -framework OpenGL -framework CoreVideo -framework Cocoa -framework IOKit -framework OpenCL -framework ForceFeedback

_WIN_OBJS = $(OBJS) glew.o win32.o gfx_gl_win.o win32.res
_LIN_OBJS = $(OBJS) glew.o linux_xlib.o gfx_gl_lin.o
_MAC_OBJS = $(OBJS) osx.o


include deps/dpb/src/Makefile


# this has to list everything inside the app bundle
$(MAC_CONTENTS)/_CodeSignature/CodeResources : \
	$(MAC_CONTENTS)/MacOS/$(BINARY_NAME) \
	$(MAC_CONTENTS)/Resources/AppIcon.icns \
	$(MAC_CONTENTS)/Info.plist
	codesign --force --deep --sign - $(BINARY_NAME).app

# copies the binary, and tells it where to find libraries
$(MAC_CONTENTS)/MacOS/$(BINARY_NAME): $(BINARY_NAME).bin
	@mkdir -p $(MAC_CONTENTS)/MacOS
	cp $< $@
# end build the App Bundle

