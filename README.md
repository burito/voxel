Voxel
=====
This is/will be a real-time OpenGL Sparse Voxel Oct-tree Raycaster.
It makes heavy use of GPU compute.

Build Environment
=================
Windows
-------
* Install [mingw-w64-install.exe](http://sourceforge.net/projects/mingw-w64/files/)
* Install http://msysgit.github.io/
* Put their ``/bin`` directories in your ``PATH``.
* Mingw will now link directly against DLL's, no more library required.
* Source the [OpenCL header files](https://github.com/KhronosGroup/OpenCL-Headers), give them to your compiler.
* Install [ImageMagick](http://www.imagemagick.org/script/binary-releases.php#windows)

Mac OS X Yosemite
-----------------
* Install [XCode](https://developer.apple.com/xcode/downloads/)


Debian & Ubuntu
---------------
    sudo apt-get install a56 imagemagick git-core libglu1-mesa-dev libxi-dev ocl-icd-opencl-dev mingw-w64 
If cross compiling, copy the OpenCL library and headers from the Windows setup to ``/usr/x86_64-w64-mingw32/``

Build Instructions
------------------
    git clone git@github.com:burito/voxel.git		# if you're using ssh
    cd voxel
    make -j8		# if you have 8 threads

By default, it will build the binary native for your platform.

    make gui		# builds the default linux binary
    make gui.exe	# builds the default windows binary
    make gui.app	# builds the default OSX binary
    make voxel.zip  # builds the distributable

Usage
-----
* ESC quits.
* WASD keys move around.
* CTRL goes down, SPACE goes up.
* Holding Right Mouse aims the camera.
* F11 toggles Fullscreen. (⌃⌘F on Mac)

System Requirements
-------------------
* Needs OpenGL 4.3 or OpenCL 1.1
* Wants 64bit, but it's not needed. Yet.
* You'll need up to date drivers. As of now that means Nvidia 347.25

The Mac version has no GL renderer mode as [OSX does not support OpenGL 4.3](https://developer.apple.com/graphicsimaging/opengl/capabilities/).

Afterword
---------
It's a tech demo, don't expect it to work fast or at all.
It only works on NVidia GPU's, I'm told it works on a GT240, but I
use a GTX680, and advise you use something faster. AMD support is almost totally
broken. It needs a miracle from AMD's driver team, not likely.

-Dan

Please refer to the articles at...

http://danpburke.blogspot.com.au

...for the complete set of ramblings.

