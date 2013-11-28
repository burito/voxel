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
#include <string.h>

#include "text.h"
#include "shader.h"

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


static void printProgramInfoLog(GLuint obj)
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


static GLuint shader_fileload(int type, char * filename)
{
	GLuint x;
	int param;
	char *buf;

	buf = loadTextFile(filename);
	if(!buf)return 0;
	x = glCreateShader(type);
	glShaderSource(x, 1, (const GLchar**)&buf, NULL);
	free(buf);
	glCompileShader(x);
	glGetShaderiv(x, GL_COMPILE_STATUS, &param);
	if(param == GL_FALSE)
	{
		printf("*** Shader compile of \"%s\" went as expected.\n", filename);
		printShaderInfoLog(x);
		glDeleteShader(x);
		x = 0;
	}
	return x;
}



void shader_rebuild(GLSLSHADER *s)
{
	if(s->vert) { glDeleteShader(s->vert); s->vert = 0; }
	if(s->frag) { glDeleteShader(s->frag); s->frag = 0; }
	if(s->prog) { glDeleteProgram(s->prog);s->prog = 0; }
	s->happy = 0;

	s->prog = glCreateProgram();
	if(s->fragfile)
	{
		s->vert = shader_fileload(GL_VERTEX_SHADER, s->vertfile);
		s->frag = shader_fileload(GL_FRAGMENT_SHADER, s->fragfile);
		if(!s->vert)return;
		if(!s->frag)return;
		glAttachShader(s->prog, s->vert);
		glAttachShader(s->prog, s->frag);
	}
	else
	{
		s->vert = shader_fileload(GL_COMPUTE_SHADER, s->vertfile);
		if(!s->vert)return;
		glAttachShader(s->prog, s->vert);
	}
	glLinkProgram(s->prog);
	GLint param;
	glGetProgramiv(s->prog, GL_LINK_STATUS, &param);
	if(param == GL_FALSE)
	{
		printf("*** Shader linking went as expected.\n");
		printProgramInfoLog(s->prog);
	}
	else
	{
		s->happy = 1;
	}
}


GLSLSHADER* shader_load(char *vertfile, char *fragfile)
{
	GLSLSHADER *s = malloc(sizeof(GLSLSHADER));
	memset(s, 0, sizeof(GLSLSHADER));
	s->vertfile = hcopy(vertfile);
	s->fragfile = hcopy(fragfile);
	shader_rebuild(s);
	return s;
}


