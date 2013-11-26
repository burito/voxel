/*
Copyright (c) 2013 Daniel Burke

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

typedef struct GLSLVOXEL
{
	GLuint render, r_frag, r_vert;
	GLint time, depth;
	char *filename;
	int happy;

} GLSLVOXEL;


extern int use_glsl;

void voxel_init(void);
void voxel_loop(void);
void voxel_end(void);
void voxel_rebuild(widget*);
void voxel_open(char *filename);

void voxel_rebuildkernel(widget* foo);
void voxel_rebuildshader(widget* foo);

