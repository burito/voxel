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

typedef struct GLSLSHADER
{
	GLuint prog, frag, vert;
	int unif_num;
	GLint *unif;
	char **unif_name;
	int buf_num;
	GLint *buf;
	char **buf_name;
	char *fragfile, *vertfile;
	int happy;
} GLSLSHADER;


GLSLSHADER* shader_load(char *vertfile, char * fragfile);
void shader_uniform(GLSLSHADER *s, char *name);
void shader_buffer(GLSLSHADER *s, char *name);
void shader_rebuild(GLSLSHADER *s);
void shader_free(GLSLSHADER *s);

char* glError(int error);

extern char *shader_header;
int available_vram(void);
GLint glBaseFormat(GLint);
GLint glBaseType(GLint);

