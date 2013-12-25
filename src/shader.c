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

static void shader_unifind(GLSLSHADER *s)
{
	if(!s)return;
	if(!s->happy)return;
	for(int i=0; i<s->unif_num; i++)
	{
		s->unif[i] = glGetUniformLocation(s->prog, s->unif_name[i]);
		if(-1 == s->unif[i])
		{
			printf("Shader(\"%s\")uniform(\"%s\"):Not Found\n",
				s->fragfile, s->unif_name[i]);
		}
	}
}

static void shader_buffind(GLSLSHADER *s)
{
	if(!s)return;
	if(!s->happy)return;
	for(int i=0; i<s->buf_num; i++)
	{
		s->buf[i] = glGetUniformBlockIndex(s->prog, s->buf_name[i]);
		glUniformBlockBinding(s->prog, s->buf[i], i);
		if(-1 == s->buf[i])
		{
			printf("Shader(\"%s\")block(\"%s\"):Not Found\n",
				s->fragfile, s->buf_name[i]);
		}
	}
}




void shader_rebuild(GLSLSHADER *s)
{
	if(s->vert) { glDeleteShader(s->vert); s->vert = 0; }
	if(s->frag) { glDeleteShader(s->frag); s->frag = 0; }
	if(s->prog) { glDeleteProgram(s->prog);s->prog = 0; }
	s->happy = 0;

	s->prog = glCreateProgram();
	if(s->vertfile)
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
		s->frag = shader_fileload(GL_COMPUTE_SHADER, s->fragfile);
		if(!s->frag)return;
		glAttachShader(s->prog, s->frag);
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
		shader_unifind(s);
		shader_buffind(s);
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


void shader_uniform(GLSLSHADER *s, char *name)
{
	if(!s)return;
	int i = s->unif_num++;
	void* tmp;
	// Realloc the name array
	tmp = realloc(s->unif_name, sizeof(char*)*s->unif_num);
	if(!tmp)printf("Realloc() failed\n");
	s->unif_name = tmp;
	s->unif_name[i] = hcopy(name);
	// realloc the uniform location id array
	tmp = realloc(s->unif, sizeof(GLint)*s->unif_num);
	if(!tmp)printf("Realloc() failed\n");
	s->unif = tmp;
	s->unif[i] = glGetUniformLocation(s->prog, name);
	// Check if we found the uniform
	if(-1 == s->unif[i])
	{
		printf("Shader(\"%s\")uniform(\"%s\"):Not Found\n",
				s->fragfile, s->unif_name[i]);
	}
}


void shader_buffer(GLSLSHADER *s, char *name)
{
	if(!s)return;
	int i = s->buf_num++;
	void* tmp;
	// Realloc the name array
	tmp = realloc(s->buf_name, sizeof(char*)*s->buf_num);
	if(!tmp)printf("Realloc() failed\n");
	s->buf_name = tmp;
	s->buf_name[i] = hcopy(name);
	// realloc the uniform location id array
	tmp = realloc(s->buf, sizeof(GLint)*s->buf_num);
	if(!tmp)printf("Realloc() failed\n");
	s->buf = tmp;
	s->buf[i] = glGetUniformBlockIndex(s->prog, name);
	// Check if we found the uniform
	if(-1 == s->buf[i])
	{
		printf("Shader(\"%s\")buffer(\"%s\"):Not Found\n",
				s->fragfile, s->buf_name[i]);
	}
	else
	{
		glUniformBlockBinding(s->prog, s->buf[i], i);
	}
}



