#include <stdio.h>			// malloc, free, fopen, fclose, ftell, fseek, fread
#include <stdlib.h>
#include <string.h>			// memset
#define FONTSTASH_IMPLEMENTATION	// Expands implementation

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


#include "fontstash.h"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/glew.h>
#endif

#define GLFONTSTASH_IMPLEMENTATION	// Expands implementation
#include "glfontstash.h"