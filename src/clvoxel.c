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
#include <GL/glew.h>

#include <stdio.h>
#include <string.h>

#include "main.h"
#include "3dmaths.h"
#include "gui.h"
#include "clvoxel.h"
#include "shader.h"
#include "text.h"
#include "mesh.h"


int b_size = 8;
int b_edge = 64;
#define B_COUNT (b_edge*b_edge*b_edge)
int np_size = 100000;

int voxel_rebuildshader_flag=0;

GLSLSHADER *s_Voxel=NULL;
GLuint v_nn, v_nb, v_nut, v_nrt, v_brt, v_but;
GLSLSHADER *s_Brick=NULL;
GLSLSHADER *s_BrickDry=NULL;
GLSLSHADER *s_NodeClear=NULL;
GLSLSHADER *s_NodeTerminate=NULL;
GLSLSHADER *s_NodeLRUReset=NULL;
GLSLSHADER *s_NodeLRUSort=NULL;
GLSLSHADER *s_BrickLRUReset=NULL;
GLSLSHADER *s_BrickLRUSort=NULL;
GLSLSHADER *s_NodeAlloc=NULL;
GLSLSHADER *s_BrickAlloc=NULL;
GLuint atomics;
GLuint atomic_read;

int used_nodes, used_bricks;

GLuint t3DBrick;
GLuint t3DBrickColour;
GLuint bCamera;
GLuint bNN;		// Node Pool
GLuint bNB;		// Node Brick
GLuint bNRT;	// Node Req Time
GLuint bBRT;	// Brick Req Time
GLuint bNUT;	// Node Use Time
GLuint bNLU;	// Node LRU
GLuint bNLO;	// Node LRU Output
GLuint bBUT;	// Brick Use Time
GLuint bBLU;	// Brick LRU
GLuint bBLO;	// Brick LRU Output
GLuint bNL;		// Node Locality
GLuint bBL;		// Brick Locality


#define FBUFFER_SIZE	4196
GLuint fbuffer;

int frame = 0;
int odd_frame = 0;

WF_OBJ * vobj=NULL;

int texdraw;
int populate = 1;


void voxel_rebuildshader(widget* foo)
{
	voxel_rebuildshader_flag = 1;
}

GLuint vox_3Dtex(int3 size, int format, int interp)
{
	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_3D, id);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, interp);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, interp);
//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexImage3D(GL_TEXTURE_3D, 0, format, size.x, size.y, size.z, 0,
			glBaseFormat(format), glBaseType(format), NULL);
//	printf("Error = \"%s\"\n", glError(glGetError()));
	glBindTexture(GL_TEXTURE_3D, 0);
	return id;
}


GLuint vox_glbuf(int size, void *ptr)
{
	GLuint buf;
	glGenBuffers(1, &buf);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, ptr, GL_DYNAMIC_COPY);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	return buf;
}

float texdepth= 0;
void voxel_3dtexdraw(void)
{
	if(keys[KEY_M])
	{
		if(texdepth < 511.0/512.0) texdepth += 1.0 / 512;
		printf("%d\n", (int)(texdepth*512.0));
	}
	if(keys[KEY_N])
	{
		if(texdepth > 0.0) texdepth -= 1.0 / 512;
		printf("%d\n", (int)(texdepth*512.0));
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, t3DBrick);
	glEnable(GL_TEXTURE_3D);

	float toff = 0.5 / 512.0;
	glBegin(GL_QUADS);
	glTexCoord3f(0, 0, texdepth+toff); glVertex2f(1, 0);
	glTexCoord3f(1, 0, texdepth+toff); glVertex2f(2, 0);
	glTexCoord3f(1, 1, texdepth+toff); glVertex2f(2, 1);
	glTexCoord3f(0, 1, texdepth+toff); glVertex2f(1, 1);
	glEnd();

	glDisable(GL_TEXTURE_3D);
	glBindTexture(GL_TEXTURE_3D, 0);
}




/* set the atomic vars used in compute shaders */
void voxel_atom(int x, int y)
{
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomics);
	GLuint b[] = {x, y};
	glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, 8, b);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
}


void voxel_NodeClear(void)
{
	glUseProgram(s_NodeClear->prog);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bNN); // nn
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bNB); // nb
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bNL); // nl
	glDispatchCompute(np_size / 10, 10, 1);
	glUseProgram(0);
}

void voxel_NodeTerminate(void)
{
	glUseProgram(s_NodeTerminate->prog);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bNN); // nn
	glDispatchCompute(np_size/10, 10, 8);
	glUseProgram(0);
}


void voxel_NodeLRUReset(int frame)
{
	glUseProgram(s_NodeLRUReset->prog);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bNUT); // nut
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bNLU); // nLRU
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bNRT); // nrt
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bBRT); // brq
	glUniform1i(s_NodeLRUReset->unif[0], frame);
	glDispatchCompute(np_size / 10, 10, 1);
	glUseProgram(0);
}


void voxel_BrickLRUReset(int frame)
{
	glUseProgram(s_BrickLRUReset->prog);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bBUT); // but
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bBLU); // bLRU
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bBL); // bL
	glUniform1i(s_BrickLRUReset->unif[0], frame);
	glDispatchCompute(b_edge, b_edge, b_edge);
	glUseProgram(0);
}


void voxel_NodeLRUSort(int frame)
{
	glUseProgram(s_NodeLRUSort->prog);
	voxel_atom(0, 0);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomics);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bNUT); // nut
	if(!odd_frame)
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bNLU); // nLRU
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bNLO); // nLRUo
	}
	else
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bNLO); // nLRUo
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bNLU); // nLRU
	}
	glUniform1i(s_NodeLRUSort->unif[0], frame);
	glDispatchCompute(np_size/10, 10, 1);
	glUseProgram(0);
}


void voxel_BrickLRUSort(int frame)
{
	glUseProgram(s_BrickLRUSort->prog);
	voxel_atom(0, 0);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomics);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bBUT); // but
	if(!odd_frame)
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bBLU); // bLRU
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bBLO); // bLRUo
	}
	else
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bBLO); // bLRUo
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bBLU); // bLRU
	}
	glUniform1i(s_BrickLRUSort->unif[0], frame);
	glDispatchCompute(b_edge, b_edge, b_edge);
	glUseProgram(0);
}


void voxel_NodeAlloc(int frame)
{
	glUseProgram(s_NodeAlloc->prog);
	voxel_atom(0, 0);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomics);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bNN); // nn
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bNRT); // nrt
	if(!odd_frame)
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bNLU); // nLRU
	}
	else
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bNLO); // nLRUo

	}
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bNL); // nl
	glUniform1i(s_NodeAlloc->unif[0], frame);
	glDispatchCompute(np_size/10, 10, 8);
	glUseProgram(0);
}


void voxel_BrickAlloc(int frame)
{
	glUseProgram(s_BrickAlloc->prog);
	voxel_atom(0, 0);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomics);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bNB); // nb
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bBRT); // brt
	if(!odd_frame)
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bBLU); // bLRU
	}
	else
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bBLO); // bLRUo
	}
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bBL); // bl
	glUniform1i(s_BrickAlloc->unif[0], frame);
	
	glActiveTexture(GL_TEXTURE0 + 4);
	glBindTexture(GL_TEXTURE_3D, t3DBrick);
	glBindImageTexture(4, t3DBrick, 0, /*layered=*/GL_TRUE, 0,
	GL_WRITE_ONLY, GL_RGBA16F);
	glUniform1i(s_BrickAlloc->unif[1], 4);

	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_3D, t3DBrickColour);
	glBindImageTexture(5, t3DBrickColour, 0, /*layered=*/GL_TRUE, 0,
	GL_WRITE_ONLY, GL_RGBA8);
	glUniform1i(s_BrickAlloc->unif[2], 5);

	glDispatchCompute(np_size/10, 10, 8);
	glUseProgram(0);
}


void voxel_Brick(int depth, int pass)
{
	glUseProgram(s_Brick->prog);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bNN); // nn
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bNB); // nb
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bBRT); // nb
	glUniform1i(s_Brick->unif[0], depth);
	glUniform1i(s_Brick->unif[1], frame);
	glActiveTexture(GL_TEXTURE0 + 4);
	glBindTexture(GL_TEXTURE_3D, t3DBrick);
	glBindImageTexture(4, t3DBrick, 0, /*layered=*/GL_TRUE, 0,
	GL_WRITE_ONLY, GL_RGBA16F);
	glUniform1i(s_Brick->unif[2], 4);

	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_3D, t3DBrickColour);
	glBindImageTexture(5, t3DBrickColour, 0, /*layered=*/GL_TRUE, 0,
	GL_WRITE_ONLY, GL_RGBA8);
	glUniform1i(s_Brick->unif[3], 5);

}


void voxel_BrickDry(int frame, int depth)
{
	glUseProgram(s_BrickDry->prog);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bNN); // nn
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bNB); // nb
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bNRT); // nrt
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bBRT); // brt
	glUniform1i(s_BrickDry->unif[0], frame);
	glUniform1i(s_BrickDry->unif[1], depth);
}


void voxel_Voxel(int frame)
{
	if(!s_Voxel)return;
	if(!s_Voxel->happy)return;
	glUseProgram(s_Voxel->prog);

	extern float4 pos, angle;

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, bCamera);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 16, &pos);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 16, 16, &angle);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	GLenum ssb = GL_SHADER_STORAGE_BUFFER;

	glBindBufferBase(ssb, 0, bCamera); // cam
	glBindBufferBase(ssb, 1, bNN); // nn
	glBindBufferBase(ssb, 2, bNB); // nb
	glBindBufferBase(ssb, 3, bNUT); // nut
	glBindBufferBase(ssb, 4, bNRT); // nrt
	glBindBufferBase(ssb, 5, bBRT); // brt
	glBindBufferBase(ssb, 6, bBUT); // but

	glUniform1i(s_Voxel->unif[0], frame);

	glActiveTexture(GL_TEXTURE0 + 4);
	glBindTexture(GL_TEXTURE_3D, t3DBrick);
	glBindImageTexture(GL_TEXTURE4, t3DBrick, 0, /*layered=*/GL_TRUE, 0,
	GL_READ_ONLY, GL_RGBA16F);
	glUniform1i(s_Voxel->unif[1], 4);

	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_3D, t3DBrickColour);
	glBindImageTexture(GL_TEXTURE5, t3DBrickColour, 0, /*layered=*/GL_TRUE, 0,
	GL_READ_ONLY, GL_RGBA8);
	glUniform1i(s_Voxel->unif[2], 5);

}

void print_int(GLuint id)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
	

	int* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	printf("%d=(", id);
	for(int i=0; i<12; i++)
	{
		printf("%d,", p[i]);
	}
	printf(")\n");
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}




void print_nalloc(GLuint id)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
	

	int* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	printf("%d=(", id);
	for(int i=0; i<np_size; i++)
	{
		if(p[i])printf("%d,", p[i]);
	}
	printf(")\n");
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
void print_salloc(GLuint id)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
	

	int* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	printf("%d=(", id);
	for(int i=0; i<np_size*8; i++)
	{
		if(p[i])printf("%d,", p[i]);
	}
	printf(")\n");
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}


void print_osalloc(GLuint id)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
	

	int* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	printf("%d=(", id);
	for(int i=0; i<np_size*8; i++)
	{
		if(p[i])
		{
			if(p[i]%2)
				printf("%d,", p[i]);

		}
	}
	printf(")\n");
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void print_esalloc(GLuint id)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
	

	int* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	printf("%d=(", id);
	for(int i=0; i<np_size*8; i++)
	{
		if(p[i])
		{
			if(!(p[i]%2))
				printf("%d,", p[i]);

		}
	}
	printf(")\n");
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void print_breq(int step)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, bBRT);
	int count=0;
	int* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	for(int i=0; i<np_size*8; i++)
	{
		if(p[i] == frame)
			count++;
	}
	printf("Current(%d) Requests(b=%d,", step, count);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, bNRT);
	count=0;
	p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	for(int i=0; i<np_size*8; i++)
	{
		if(p[i] == frame)
			count++;
	}
	printf("%d)\n", count);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void print_balloc(void)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, bNN);
	int* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	int countn=0;
	for(int i=0; i<np_size*8; i++)
	{
		if(p[i] > 0)countn++;
	}
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


	glBindBuffer(GL_SHADER_STORAGE_BUFFER, bNB);
	p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	int countb=0;
	for(int i=0; i<np_size*8; i++)
	{
		if(p[i])countb++;
	}
	printf("Allocated b=%d, n=%d\n", countb, countn);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

int tree_depth = 5;
int vdepth = 0;
int total_vram = 0;


void voxel_init(void)
{
	frame = 1;
	total_vram = available_vram();
	used_nodes = used_bricks = 0;
	
	size_t size = np_size*8*4;
	int b_edge = 64;
	int b_size = 8;

	printf("Total vram = %dk\n", total_vram);

	if(total_vram > 1200000) // 1.2gb of vram?
	{
		shader_header =
		"#define B_SIZE 8\n#define B_EDGE 64\n#define NP_SIZE 100000\n";
	}
	else
	{
		shader_header =
		"#define B_SIZE 8\n#define B_EDGE 48\n#define NP_SIZE 100000\n";
		b_edge = 48;
	}

	int cs = b_size * b_edge;
	int3 cube = {cs, cs, cs};
	t3DBrick = vox_3Dtex(cube, GL_RGBA16F, GL_LINEAR);
	t3DBrickColour = vox_3Dtex(cube, GL_RGBA8, GL_LINEAR);
	bCamera = vox_glbuf(sizeof(float)*8, NULL);	// camera
	bNN = vox_glbuf(size, NULL);	// NodeNode
	bNL = vox_glbuf(size, NULL);	// NodeLocality
	bNB = vox_glbuf(size, NULL);	// NodeBrick
	bNRT = vox_glbuf(size, NULL);	// NodeReqTime
	bBRT = vox_glbuf(size, NULL);	// BrickReqTime
	bNUT = vox_glbuf(np_size*4, NULL);	// NodeUseTime
	bNLU = vox_glbuf(np_size*4, NULL);	// NodeLRU
	bNLO = vox_glbuf(np_size*4, NULL);	// NodeLRUOut
	bBUT = vox_glbuf(B_COUNT*4, NULL);	// BrickUseTime
	bBLU = vox_glbuf(B_COUNT*4, NULL);	// BrickLRU
	bBLO = vox_glbuf(B_COUNT*4, NULL);	// BrickLRUOut
	bBL = vox_glbuf(B_COUNT*4, NULL);	// BrickLocality


	s_Voxel = shader_load("data/shaders/Vertex.GLSL","data/shaders/Voxel.GLSL");
	s_Brick = shader_load("data/shaders/Vertex.GLSL","data/shaders/Brick.GLSL");
	s_BrickDry = shader_load("data/shaders/Vertex.GLSL",
							"data/shaders/BrickDry.GLSL");
	s_NodeClear = shader_load(0, "data/shaders/NodeClear.GLSL");
	s_NodeTerminate = shader_load(0, "data/shaders/NodeTerminate.GLSL");
	s_NodeLRUReset = shader_load(0, "data/shaders/NodeLRUReset.GLSL");
	s_NodeLRUSort = shader_load(0, "data/shaders/NodeLRUSort.GLSL");
	s_BrickLRUReset = shader_load(0, "data/shaders/BrickLRUReset.GLSL");
	s_BrickLRUSort = shader_load(0, "data/shaders/BrickLRUSort.GLSL");
	s_NodeAlloc = shader_load(0, "data/shaders/NodeAlloc.GLSL");
	s_BrickAlloc = shader_load(0, "data/shaders/BrickAlloc.GLSL");

	
	shader_uniform(s_Voxel, "time");
	shader_uniform(s_Voxel, "bricks");
	shader_uniform(s_Voxel, "brick_col");
	shader_uniform(s_Brick, "depth");
	shader_uniform(s_Brick, "time");
	shader_uniform(s_Brick, "bricks");
	shader_uniform(s_Brick, "brick_col");
	shader_uniform(s_Brick, "pass");
	shader_uniform(s_BrickDry, "time");
	shader_uniform(s_BrickDry, "depth");
	shader_uniform(s_NodeLRUReset, "time");
	shader_uniform(s_NodeLRUSort, "time");
	shader_uniform(s_BrickLRUReset, "time");
	shader_uniform(s_BrickLRUSort, "time");
	shader_uniform(s_NodeAlloc, "time");
	shader_uniform(s_BrickAlloc, "time");
	shader_uniform(s_BrickAlloc, "bricks");
	shader_uniform(s_BrickAlloc, "brick_col");

	odd_frame = 0;
	voxel_NodeClear();
	voxel_NodeLRUReset(frame);
	voxel_BrickLRUReset(frame);

	glGenBuffers(1, &atomics);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomics);
	glBufferData(GL_ATOMIC_COUNTER_BUFFER,
			sizeof(GLuint)*2, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

	atomic_read = vox_glbuf(8, NULL);

	glGenFramebuffers(1, &fbuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, fbuffer);
	glFramebufferParameteri(GL_FRAMEBUFFER, 
			GL_FRAMEBUFFER_DEFAULT_WIDTH, FBUFFER_SIZE);
	glFramebufferParameteri(GL_FRAMEBUFFER,
			GL_FRAMEBUFFER_DEFAULT_HEIGHT, FBUFFER_SIZE);
	glFramebufferParameteri(GL_FRAMEBUFFER,GL_FRAMEBUFFER_DEFAULT_SAMPLES, 0);
	glFramebufferParameteri(GL_FRAMEBUFFER,GL_FRAMEBUFFER_DEFAULT_LAYERS, 0);
	GLenum err;
	err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(err != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("Creating Empty Framebuffer failed %s\n", glError(err));

	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void print_atoms(void)
{
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomics);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomic_read);

	glCopyBufferSubData(GL_ATOMIC_COUNTER_BUFFER,
			GL_SHADER_STORAGE_BUFFER, 0, 0, 8);
	GLuint atm[2];
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 8, atm);

	printf("Atomics %d, %d\n", atm[0], atm[1]);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
}

int2 get_atoms(void)
{
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomics);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomic_read);

	glCopyBufferSubData(GL_ATOMIC_COUNTER_BUFFER,
			GL_SHADER_STORAGE_BUFFER, 0, 0, 8);
	int2 atm;
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 8, (GLuint*)&atm);

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
	return atm;
}


extern float4 pos;
extern float4 angle;


void voxel_loop(void)
{
	frame++;

	if(keys[KEY_V])
	{
		keys[KEY_V] = 0;
		if(tree_depth > 1)tree_depth--;
		printf("tree depth=%d\n", tree_depth);
	}
	if(keys[KEY_B])
	{
		keys[KEY_B] = 0;
		if(tree_depth < 20)tree_depth++;
		printf("tree depth=%d\n", tree_depth);
	}

	if(keys[KEY_Z])
	{
		keys[KEY_Z] = 0;
		voxel_rebuildshader_flag = 1;
	}

	if(voxel_rebuildshader_flag)
	{
		voxel_rebuildshader_flag = 0;
		shader_rebuild(s_Voxel);
		shader_rebuild(s_Brick);
	}

	if(keys[KEY_C])
	{
		keys[KEY_C] = 0;
		voxel_open();

	}

	if(keys[KEY_X])
	{
		keys[KEY_X] = 0;
		print_atoms();

	}



	float tleft, tright;
	float ttop, tbottom;


	voxel_Voxel(frame);
	tleft = 0; tright = 1;
	float srat = ((float)vid_height / (float)vid_width) * 0.5;
	ttop = 0.5 - srat;
	tbottom = 0.5 + srat;

	float left = 0, right = vid_width;
	float top = 0, bottom = vid_height;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, vid_width, 0, vid_height, -1000, 5000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glColor4f(1,1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(tleft, tbottom); glVertex2f(left, top);
	glTexCoord2f(tright, tbottom); glVertex2f(right, top);
	glTexCoord2f(tright, ttop); glVertex2f(right, bottom);
	glTexCoord2f(tleft, ttop); glVertex2f(left, bottom);
	glEnd();


	glUseProgram(0);


	
//		glMemoryBarrier(GL_ALL_BARRIER_BITS);
	//	if(vobj)print_breq(frame);

//		voxel_NodeTerminate();
	if(vobj)
	{
//		glMemoryBarrier(GL_ALL_BARRIER_BITS);
		voxel_NodeLRUSort(frame);
		used_nodes = get_atoms().x;
//		glMemoryBarrier(GL_ALL_BARRIER_BITS);
//		print_atoms();
		voxel_BrickLRUSort(frame);
		used_bricks = get_atoms().x;
//		glMemoryBarrier(GL_ALL_BARRIER_BITS);
//		print_atoms();
		odd_frame = !odd_frame;
		frame++;
	}

	if(vobj && populate)
	{
		int vwidth = b_size-1;
		for(int i = 0; i < vdepth; i++)
			vwidth = vwidth * 2;

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		float scale = FBUFFER_SIZE;
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, vwidth, 0, vwidth, -4000, 4000);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glScalef(vwidth, vwidth, vwidth);

		glBindFramebuffer(GL_FRAMEBUFFER, fbuffer);
		glViewport(0,0,vwidth, vwidth);
		voxel_BrickDry(frame, vdepth);
		// render
		vobj->draw(vobj);
		glTranslatef(0.5,0.5,0.5);
		glRotatef(90, 0, 1.0, 0);
		glTranslatef(-0.5,-0.5,-0.5);
		vobj->draw(vobj);
		glTranslatef(0.5,0.5,0.5);
		glRotatef(90, 0, 0.0, 1.0);
//		glRotatef(90, 0, 1, 0);
		glTranslatef(-0.5,-0.5,-0.5);
		vobj->draw(vobj);

		glMemoryBarrier(GL_ALL_BARRIER_BITS);
//	if(vobj)
//	{
//			print_breq(frame);
//			print_balloc();
//		}

		if(!odd_frame)
		{
			voxel_NodeAlloc(frame);
		}
		else
		{
			voxel_BrickAlloc(frame);
			glMemoryBarrier(GL_ALL_BARRIER_BITS);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glScalef(vwidth, vwidth, vwidth);
			voxel_Brick(vdepth, frame);
			glUniform1i(s_Brick->unif[4], 2);
			vobj->draw(vobj);
			glTranslatef(0.5,0.5,0.5);
			glRotatef(90, 1, 0.0, 0);
			glTranslatef(-0.5,-0.5,-0.5);
			glUniform1i(s_Brick->unif[4], 1);
			vobj->draw(vobj);
			glTranslatef(0.5,0.5,0.5);
			glRotatef(90, -1, 0.0, 0);
			glRotatef(90, 0, 1, 0);
			glTranslatef(-0.5,-0.5,-0.5);
			glUniform1i(s_Brick->unif[4], 0);
			vobj->draw(vobj);
			vdepth++;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0,0,vid_width, vid_height);
		if(frame >= (tree_depth)*4) populate = 0;

		}
	// all done
	glUseProgram(0);
	glDisable(GL_TEXTURE_3D);
//	glActiveTexture(GL_TEXTURE4);
//	glBindTexture(GL_TEXTURE_3D, 0);

	glActiveTexture(GL_TEXTURE0);
//	printf("Nodes = %d, Bricks = %d\n", used_nodes, used_bricks);

	if(texdraw)voxel_3dtexdraw();
}

void voxel_end(void)
{
	shader_free(s_Voxel);
	shader_free(s_Brick);
	shader_free(s_BrickDry);
	shader_free(s_NodeClear);
	shader_free(s_NodeTerminate);
	shader_free(s_NodeLRUReset);
	shader_free(s_NodeLRUSort);
	shader_free(s_BrickLRUReset);
	shader_free(s_BrickLRUSort);
	shader_free(s_NodeAlloc);
	shader_free(s_BrickAlloc);

	glDeleteTextures(1, &t3DBrick);
	glDeleteBuffers(1, &bCamera);
	glDeleteBuffers(1, &bNN);
	glDeleteBuffers(1, &bNB);
	glDeleteBuffers(1, &bNRT);
	glDeleteBuffers(1, &bBRT);
	glDeleteBuffers(1, &bNUT);
	glDeleteBuffers(1, &bNLU);
	glDeleteBuffers(1, &bNLO);
	glDeleteBuffers(1, &bBUT);
	glDeleteBuffers(1, &bBLU);
	glDeleteBuffers(1, &bBLO);
}


void voxel_open(void)
{
	populate = 1;
	frame = 0;
	odd_frame = 0;
	vdepth = 1;
	voxel_NodeClear();
	voxel_NodeLRUReset(frame);
	voxel_BrickLRUReset(frame);
}


