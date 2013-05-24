
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <GL/glew.h>
#include <GL/gl.h>

void msg(const char *string, ...)
{
	char mesg[1000];
	memset(mesg, 0, 1000);
	va_list marker;
	va_start( marker, string );
	vsnprintf( mesg , 999, string, marker);
	va_end( marker );
	printf(mesg);
}

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

// Copy a zero terminated string onto the heap
char* hcopy(const char *string)
{
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


