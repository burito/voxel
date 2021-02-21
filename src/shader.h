/*
Copyright (c) 2012,2020 Daniel Burke

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

struct GLSLSHADER
{
	// A flag to let us know if everything is in order
	int happy;

	// for the GL id's of the 3 components
	GLuint program;
	GLuint fragment;
	GLuint vertex;

	// the paths to the fragment shader files
	char *vertex_filename;
	char *fragment_filename;

	int uniform_num;
	GLint *uniforms;
	char **uniform_names;
	int buffer_num;
	GLint *buffers;
	char **buffer_names;

};


struct GLSLSHADER* shader_load(char *vertex_filename, char *fragment_filename);
void shader_uniform(struct GLSLSHADER *s, char *name);
void shader_buffer(struct GLSLSHADER *s, char *name);
void shader_rebuild(struct GLSLSHADER *s);
void shader_free(struct GLSLSHADER *s);

char* glError(int error);

extern char *shader_header;
int available_vram(void);
GLint glBaseFormat(GLint);
GLint glBaseType(GLint);
