Voxel
=====
This is/will be a real-time OpenGL Sparse Voxel Oct-tree Raycaster.
It makes heavy use of GPU compute.

Build Environment
-----------------
Setting up the build environment for Windows and Debian is detailed on [my
blog](http://danpburke.blogspot.com.au/2013/11/shadertoy-like-opencl-kernel-tester.html).

Build Instructions
------------------
    make -j8		# if you have 8 threads, alter to taste

Usage
-----
* ESC quits.
* WASD keys move around.
* CTRL goes down, SPACE goes up.
* Holding Right Mouse aims the camera.
* F11 toggles Fullscreen.

System Requirements
-------------------
* Needs OpenGL 4.3
* Wants 64bit, but it's not needed. Yet.
* You'll need up to date drivers. As of now that means Nvidia 337.25.
* 1Gb VRAM minimum, 2Gb wanted.

There is no Mac support, [OSX does not support OpenGL
4.3.](https://developer.apple.com/graphicsimaging/opengl/capabilities/)


Afterword
---------
It's a tech demo, don't expect it to work fast or at all.
I test it on my Nvidia GTX680. At the moment there's no reason it
shouldn't work on AMD GPU's, although I'm fairly deliberately playing fast and
loose with the OpenGL standard, and AMD's drivers tend to be stricter than
Nvidias.

-Dan

Please refer to the articles at...

http://danpburke.blogspot.com.au

...for the complete set of ramblings.


