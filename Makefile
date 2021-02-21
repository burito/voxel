
COMPANY = Daniel Burke
COPYRIGHT = 2013-2019
DESCRIPTION = Voxel Prototype
BINARY_NAME = voxel
OBJS = main.o version.o log.o global.o 3dmaths.o glerror.o vr.o \
	shader.o fps_movement.o mesh.o mesh_gl.o shader.o \
	stb_ds.o stb_image.o stb_truetype.o fontstash.o \
	gpuinfo.o vr.o vr_helper.o fps_movement.o spacemouse.o \
	clvoxel.o gui.o

CFLAGS = -std=c11 -Wall -isystem deps -Ideps/dpb/src -Ideps/dpb/deps/hidapi/hidapi
VPATH = src build deps deps/dpb/src deps/dpb/deps deps/dpb/deps/hidapi/

WIN_LIBS = -luser32 -lwinmm -lgdi32 -lshell32 -lopengl32 -lws2_32 -lxinput9_1_0
LIN_LIBS = -lm -lGL -lX11 -lGLU -lXi -ldl
MAC_LIBS = -F/System/Library/Frameworks -F. -framework OpenGL -framework CoreVideo -framework Cocoa -framework IOKit -framework ForceFeedback

_WIN_OBJS = $(OBJS) glew.o win32.o gfx_gl_win.o win32.res windows/hid.o
_LIN_OBJS = $(OBJS) glew.o linux_xlib.o gfx_gl_lin.o linux/hid.o
_MAC_OBJS = $(OBJS) osx.o gfx_gl_osx.o mac/hid.o voxel.o


include deps/dpb/src/Makefile

$(BINARY_NAME).exe: $(WIN_OBJS) openvr_api.dll

$(BINARY_NAME): $(LIN_OBJS) libopenvr_api.so

$(BINARY_NAME).bin: $(MAC_OBJS) libopenvr_api.dylib


openvr_api.dll: deps/openvr/bin/win64/openvr_api.dll
	cp $< $@

libopenvr_api.dylib: deps/openvr/bin/osx32/libopenvr_api.dylib
	cp $< $@

libopenvr_api.so: deps/openvr/bin/linux64/libopenvr_api.so
	cp $< $@


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
