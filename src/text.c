/*
Copyright (c) 2012 Daniel Burke

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

#include <GL/glew.h>


/* Produces a string from the glGetError() return value */
// http://www.opengl.org/sdk/docs/man4/xhtml/glGetError.xml
char* glError(int error)
{
	switch(error)
	{
		case GL_NO_ERROR:
			return "GL_NO_ERROR";
		case GL_INVALID_ENUM:
			return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE:
			return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION:
			return "GL_INVALID_OPERATION";
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			return "GL_INVALID_FRAMEBUFFER_OPERATION";
		case GL_OUT_OF_MEMORY:
			return "GL_OUT_OF_MEMORY";
		case GL_STACK_UNDERFLOW:
			return "GL_STACK_UNDERFLOW";
		case GL_STACK_OVERFLOW:
			return "GL_STACK_OVERFLOW";
		case GL_TABLE_TOO_LARGE: // defined where?
			return "GL_TABLE_TOO_LARGE";
		default:
			return "Unknown Error!";
	}
}

void tailchomp(char *string)
{
	for(int i = strlen(string); i>0; i--)
	switch(string[i]) {
	case '\r':
	case '\n':
	case '\t':
	case ' ':
	case 0:
		string[i] = 0;
		break;
	default:
		return;
	}
}

int whitespace(char c)
{
	switch(c) {
	case ' ':
	case '\t':
		return 1;
	}
	return 0;
}


int eol(char c)
{
	switch(c) {
	case 0:
	case '\r':
	case '\n':
		return 1;
	default:
		return 0;
	}
}

// Copy a zero terminated string onto the heap
char* hcopy(const char *string)
{
	if(!string)return 0;
	int len = strlen(string);
	char * ret = malloc(len+1);
	if(!ret)return 0;
	memcpy(ret, string, len);
	ret[len]=0;
	return ret;
}


char* repath(const char *hostpath, const char *file)
{
	int len = strlen(hostpath);
	int i;
	char *ret;
	int lenfile = strlen(file);

	for(i=len; i>0; i--)
		if( hostpath[i] == '/' || hostpath[i] == '\\' )
			break;

	if(!i)return hcopy(file);

	i++;
	ret = malloc(i + lenfile + 1);
	if(!ret)return 0;

	memcpy(ret, hostpath, i);

	memcpy(ret+i, file, lenfile);

	ret[i+lenfile] = 0;
	return ret;
}


char* loadTextFile(char *filename)
{
	FILE *fptr = fopen(filename, "rb");
	if(!fptr)return 0;
	struct stat stbuf;
	stat(filename, &stbuf);
	int size = stbuf.st_size;
	char *buf = malloc(size+1);
	buf[size] = 0;
	fread(buf, size, 1, fptr);
	fclose(fptr);
	return buf;
}

#include "fast_atof.c"

