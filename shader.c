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

#include <GL/glew.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static void printShaderInfoLog(GLuint obj)
{
	int infologLength = 0;
	int charsWritten  = 0;
	char *infoLog;

	glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

	if (infologLength > 0)
	{
		infoLog = (char *)malloc(infologLength);
		glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
		printf("%s\n",infoLog);
		free(infoLog);
	}
}

void printProgramInfoLog(GLuint obj)
{
	int infologLength = 0;
	int charsWritten  = 0;
	char *infoLog;

	glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

	if (infologLength > 0)
	{
		infoLog = (char *)malloc(infologLength);
		glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
		printf("%s\n",infoLog);
		free(infoLog);
	}
}

char* file_load(char *filename)
{
	FILE *fptr;
	int size;
	struct stat stbuf;
	char *buf;

	fptr = fopen(filename, "rb");
	if(!fptr)return 0;
	stat(filename, &stbuf);
	size = stbuf.st_size;
	buf = malloc(size+1);
	fread(buf, size, 1, fptr);
	buf[size] = 0;
	fclose(fptr);
	return buf;
}

GLuint shader_load(int type, char * filename)
{
	GLuint x;
	int param;
	char *buf;

	buf = file_load(filename);
	if(!buf)return 0;
	x = glCreateShader(type);
	glShaderSource(x, 1, (const GLchar**)&buf, NULL);
	free(buf);
	glCompileShader(x);
	glGetShaderiv(x, GL_COMPILE_STATUS, &param);
	printf("*** Shader compile of \"%s\" %s.\n", filename,
			param == GL_FALSE ? "went as expected" : "worked");
	
	printShaderInfoLog(x);
	return x;
}



