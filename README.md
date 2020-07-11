# Voxel
This is a real-time OpenGL Sparse Voxel Oct-tree Raycaster.
It makes heavy use of GPU compute, so much so that a GPU temperature indicator was a needed feature. If you can smell solder, or the temperature indicator gets above 80, quit the program immediately (Press ESC).

## Quickstart
```bash
git clone --recurse-submodules git@github.com:burito/voxel
cd voxel
make -j8        # Build it using 8 threads
voxel.exe       # Windows
./voxel         # Linux
./voxel.bin     # MacOSX version doesn't work
```

Load a Wavefront model with the "Voxel Load" option, then press `C` to regenerate the tree. You can increase or decrease the depth of the tree with `B` or `V` respectively, after which you'll have to press `C` again to see the changes. It only attempts to populate the part of the tree that is visible to the camera.

## Usage
* `ESC` - quits
* `WASD` - move around
* `CTRL` - mode down
* `SPACE` - mode up
* Holding Right Mouse aims the camera.
* `F11` - toggle Fullscreen (⌃⌘F on Mac)
* `C` - regenerate the voxel tree
* `V` - decrease the voxel tree depth
* `B` - increase the voxel tree depth

## Build Environment
### Windows
* Install current Nvidia drivers (451.67)
* Install [msys2-x86_64-20200629.exe](https://www.msys2.org/)
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-imagemagick mingw-w64-x86_64-clang mingw-w64-x86_64-clang-tools-extra git msys/man-pages-posix --disable-download-timeout
```

### Linux
* Install current GPU drivers, compiler, and libraries
```bash
add-apt-repository ppa:graphics-drivers/ppa
apt update
apt install nvidia-410 vulkan-utils build-essential clang imagemagick a52 git-core libglu1-mesa-dev libxi-dev ocl-icd-opencl-dev
```

### MacOS
* Install [XCode](https://developer.apple.com/xcode/downloads/)

## System Requirements
* Needs OpenGL 4.3 or OpenCL 1.1
* Wants 64bit, but it's not needed. Yet.
* You'll need up to date drivers. As of now that means Nvidia 417.35

The Mac version has no GL renderer mode as [OSX does not support OpenGL 4.3](https://developer.apple.com/graphicsimaging/opengl/capabilities/).


## Credits
* [```deps/stb```](https://github.com/nothings/stb) - [Sean Barrett](http://nothings.org/)
* [```deps/fast_atof.c```](http://www.leapsecond.com/tools/fast_atof.c) - [Tom Van Baak](http://www.leapsecond.com/)
* [```deps/small-matrix-inverse```](https://github.com/niswegmann/small-matrix-inverse) - Nis Wegmann
* [```deps/models```](https://github.com/burito/models) - [Morgan McGuire's Computer Graphics Archive](https://casual-effects.com/data)
* ```deps/*gl*``` - [GLEW 2.1.0](http://glew.sourceforge.net/)
    * Add ```#define GLEW_STATIC``` to the top of ```glew.h```
* ```deps/*fontstash*``` - Mikko Mononen
    * https://github.com/memononen/fontstash
* ```deps/include/CL/*``` - The Khronos Group
    * https://github.com/KhronosGroup/OpenCL-Headers

This is based losely on parts of the algorithm outlined by [Cyril Crassin](http://maverick.inria.fr/Members/Cyril.Crassin/) in his [PhD Thesis](http://maverick.inria.fr/Publications/2011/Cra11/CCrassinThesis_EN_Web.pdf).
It is not a complete implementation, and diverges from it in a few ways. In other words, don't allow how bad this is to reflect on him.

For everything else, I am to blame.
