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

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define _MSC_VER 1
#include <CL/cl_gl.h>
#include <CL/cl.h>

#include <stdio.h>
#include <string.h>

#include "main.h"
#include "3dmaths.h"
#include "ocl.h"
#include "gui.h"
#include "clvoxel.h"
#include "shader.h"
#include "text.h"
#include "mesh.h"


#define B_SIZE 8
#define B_EDGE 64
#define B_COUNT (B_EDGE*B_EDGE*B_EDGE)
#define NP_SIZE 100000


OCLPROGRAM *clVox=NULL;
int voxel_rebuildkernel_flag=0;
int voxel_rebuildshader_flag=0;
int Knl_Render;
int Knl_ResetNodeTime;
int Knl_ResetBrickTime;
int Knl_NodeLRU;
int Knl_NodeAlloc;
int Knl_BrickLRU;
int Knl_BrickAlloc;

int use_glsl = 1;

GLSLSHADER *s_Voxel=NULL;
GLuint v_nn, v_nb, v_nut, v_nrt, v_brt, v_but;
GLSLSHADER *s_Brick=NULL;
GLSLSHADER *s_BrickDry=NULL;
GLSLSHADER *s_NodeClear=NULL;
GLSLSHADER *s_NodeLRUReset=NULL;
GLSLSHADER *s_NodeLRUSort=NULL;
GLSLSHADER *s_BrickLRUReset=NULL;
GLSLSHADER *s_BrickLRUSort=NULL;
GLSLSHADER *s_NodeAlloc=NULL;
GLSLSHADER *s_BrickAlloc=NULL;
GLuint atomics;

int odd_frame = 0;

WF_OBJ * vobj=NULL;

#define xNN		clVox->GLid[3]
#define xNB		clVox->GLid[4]
#define xNRT	clVox->GLid[5]
#define xBRT	clVox->GLid[6]
#define xNUT	clVox->GLid[7]
#define xNLU	clVox->GLid[8]
#define xNLO	clVox->GLid[9]
#define xBUT	clVox->GLid[10]
#define xBLU	clVox->GLid[11]
#define xBLO	clVox->GLid[12]


void voxel_rebuildkernel(widget* foo)
{
	voxel_rebuildkernel_flag = 1;
}

void voxel_rebuildshader(widget* foo)
{
	voxel_rebuildshader_flag = 1;
}


static void voxel_FindKernels(void)
{
	if(!clVox)return;
	if(!clVox->happy)return;

	for(int i=0; i<clVox->num_kernels; i++)
	{
		char buf[256];
		clGetKernelInfo(clVox->k[i], CL_KERNEL_FUNCTION_NAME, 255, buf, NULL);
		if(strstr(buf, "Render"))Knl_Render = i;
		else if(strstr(buf, "ResetNodeTime"))Knl_ResetNodeTime = i;
		else if(strstr(buf, "ResetBrickTime"))Knl_ResetBrickTime = i;
		else if(strstr(buf, "NodeLRUSort"))Knl_NodeLRU = i;
		else if(strstr(buf, "NodeAlloc"))Knl_NodeAlloc = i;
		else if(strstr(buf, "BrickLRUSort"))Knl_BrickLRU = i;
		else if(strstr(buf, "BrickAlloc"))Knl_BrickAlloc = i;
	}
}


static void voxel_ResetTime(void)
{
	// Tell the GPU to reset the time buffers
	if(!clVox->happy)return;
	glFinish();
	ocl_acquire(clVox);
	cl_kernel k = clVox->k[Knl_ResetNodeTime];
	clSetKernelArg(k, 0, sizeof(float), &time);
	clSetKernelArg(k, 1, sizeof(cl_mem), &clVox->CLmem[3]);
	clSetKernelArg(k, 2, sizeof(cl_mem), &clVox->CLmem[4]);
	clSetKernelArg(k, 3, sizeof(cl_mem), &clVox->CLmem[5]);

	size_t work_size = NP_SIZE;
	cl_int ret = clEnqueueNDRangeKernel(OpenCL->q, k, 1,NULL, &work_size,
			NULL, 0, NULL, NULL);
	if(ret != CL_SUCCESS)
	{
		clVox->happy = 0;
		printf("clEnqueueNDRangeKernel():%s\n", clStrError(ret));
	}

	// Tell the GPU to reset the brick time
	k = clVox->k[Knl_ResetBrickTime];
	clSetKernelArg(k, 0, sizeof(float), &time);
	clSetKernelArg(k, 1, sizeof(cl_mem), &clVox->CLmem[8]);
	clSetKernelArg(k, 2, sizeof(cl_mem), &clVox->CLmem[11]);

	work_size = B_COUNT;
	ret = clEnqueueNDRangeKernel(OpenCL->q, k, 1,NULL, &work_size,
			NULL, 0, NULL, NULL);
	if(ret != CL_SUCCESS)
	{
		clVox->happy = 0;
		printf("clEnqueueNDRangeKernel():%s\n", clStrError(ret));
	}
	ocl_release(clVox);
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
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, xNN); // nn
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, xNB); // nb
	glDispatchCompute(NP_SIZE / 10, 10, 1);
	glUseProgram(0);
}


void voxel_NodeLRUReset(int frame)
{
	glUseProgram(s_NodeLRUReset->prog);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, xNUT); // nut
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, xNLU); // nLRU
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, xNRT); // nrt
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, xBRT); // brq
	glUniform1i(s_NodeLRUReset->unif[0], frame);
	glDispatchCompute(NP_SIZE / 10, 10, 1);
	glUseProgram(0);
}


void voxel_BrickLRUReset(int frame)
{
	glUseProgram(s_BrickLRUReset->prog);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, xBUT); // but
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, xBLU); // bLRU
	glUniform1i(s_BrickLRUReset->unif[0], frame);
	glDispatchCompute(B_EDGE, B_EDGE, B_EDGE);
	glUseProgram(0);
}


void voxel_NodeLRUSort(int frame)
{
	glUseProgram(s_NodeLRUSort->prog);
	voxel_atom(0, 0);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomics);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, xNUT); // nut
	if(!odd_frame)
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, xNLU); // nLRU
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, xNLO); // nLRUo
	}
	else
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, xNLO); // nLRUo
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, xNLU); // nLRU
	}
	glUniform1i(s_NodeLRUSort->unif[0], frame);
	glDispatchCompute(NP_SIZE/10, 10, 1);
	glUseProgram(0);
}


void voxel_BrickLRUSort(int frame)
{
	glUseProgram(s_BrickLRUSort->prog);
	voxel_atom(0, 0);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomics);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, xBUT); // but
	if(!odd_frame)
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, xBLU); // bLRU
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, xBLO); // bLRUo
	}
	else
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, xBLO); // bLRUo
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, xBLU); // bLRU
	}
	glUniform1i(s_BrickLRUSort->unif[0], frame);
	glDispatchCompute(B_EDGE, B_EDGE, B_EDGE);
	glUseProgram(0);
}


void voxel_NodeAlloc(int frame)
{
	glUseProgram(s_NodeAlloc->prog);
	voxel_atom(0, 0);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomics);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, xNN); // nn
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, xNRT); // nrt
	if(!odd_frame)
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, xNLU); // nLRU
	}
	else
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, xNLO); // nLRUo

	}
	glUniform1i(s_NodeAlloc->unif[0], frame);
	glDispatchCompute(NP_SIZE/10, 10, 8);
	glUseProgram(0);
}


void voxel_BrickAlloc(int frame)
{
	glUseProgram(s_BrickAlloc->prog);
	voxel_atom(0, 0);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomics);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, xNB); // nb
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, xBRT); // brt
	if(!odd_frame)
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, xBLU); // bLRU
	}
	else
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, xBLO); // bLRUo
	}
	glUniform1i(s_BrickAlloc->unif[0], frame);
	glBindImageTexture(0, clVox->GLid[1], 0, /*layered=*/GL_TRUE, 0,
	GL_WRITE_ONLY, GL_RGBA8);

	glDispatchCompute(NP_SIZE/10, 10, 8);
	glUseProgram(0);
}


void voxel_Brick(int frame, int depth)
{
	glUseProgram(s_Brick->prog);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, xNN); // nn
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, xNB); // nb
	glUniform1i(s_Brick->unif[0], depth);
	glUniform1i(s_Brick->unif[1], 0);
//	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_3D);
	glBindTexture(GL_TEXTURE_3D, clVox->GLid[1]);
	glBindImageTexture(0, clVox->GLid[1], 0, /*layered=*/GL_TRUE, 0,
	GL_WRITE_ONLY, GL_RGBA8);

}


void voxel_BrickDry(int frame, int depth)
{
	glUseProgram(s_BrickDry->prog);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, xNN); // nn
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, xNB); // nb
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, xNRT); // nrt
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, xBRT); // brt
	glUniform1i(s_BrickDry->unif[0], frame);
	glUniform1i(s_BrickDry->unif[1], depth);
}


void voxel_Voxel(int frame)
{
	if(!s_Voxel)return;
	if(!s_Voxel->happy)return;
	glUseProgram(s_Voxel->prog);
	GLSLSHADER *s = s_Voxel;

	extern float4 pos, angle;

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, clVox->GLid[2]);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 16, &pos);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 16, 16, &angle);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

//		GLint x;
//		glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &x);
//		printf("max ssbb=%d\n", x);

	GLenum ssb = GL_SHADER_STORAGE_BUFFER;
//	glUniformBlockBinding(s->prog, s->buf[1], 1); // nn

	glBindBufferBase(ssb, 0, clVox->GLid[2]); // cam
	glBindBufferBase(ssb, 1, clVox->GLid[3]); // nn
	glBindBufferBase(ssb, 2, clVox->GLid[4]); // nb
	glBindBufferBase(ssb, 3, clVox->GLid[7]); // nut
	glBindBufferBase(ssb, 4, clVox->GLid[5]); // nrt
	glBindBufferBase(ssb, 5, clVox->GLid[6]); // brt
	glBindBufferBase(ssb, 6, clVox->GLid[10]); // but

/*
	glUniformBlockBinding(s->prog, s->buf[0], clVox->GLid[2]); // cam
	glUniformBlockBinding(s->prog, s->buf[1], clVox->GLid[3]); // nn
	glUniformBlockBinding(s->prog, s->buf[2], clVox->GLid[4]); // nb
	glUniformBlockBinding(s->prog, s->buf[3], clVox->GLid[7]); // nut
	glUniformBlockBinding(s->prog, s->buf[4], clVox->GLid[5]); // nrt
	glUniformBlockBinding(s->prog, s->buf[5], clVox->GLid[6]); // brt
	glUniformBlockBinding(s->prog, s->buf[6], clVox->GLid[10]); // but
*/
//		GLuint bi = glGetUniformBlockIndex(glVox->render, "camera");
//		glUniformBlockBinding(glVox->render, bi, clVox->GLid[2]);
//		glUniformBlockBinding(glVox->render, bi, GL_SHADER_STORAGE_BUFFER);
//		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glUniform1i(s_Voxel->unif[0], frame);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, clVox->GLid[1]);


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



void print_balloc(GLuint id)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
	

	int* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	printf("%d=(", id);
	for(int i=0; i<B_COUNT; i++)
	{
		if(p[i])printf("%d,", p[i]);
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
	for(int i=0; i<NP_SIZE; i++)
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
	for(int i=0; i<NP_SIZE*8; i++)
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
	for(int i=0; i<NP_SIZE*8; i++)
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
	for(int i=0; i<NP_SIZE*8; i++)
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




int frame;
void voxel_init(void)
{
	frame = 0;

	clVox = ocl_build("./data/Voxel.OpenCL");
	ocl_rm(clVox);
	if(clVox->happy)voxel_FindKernels();
	
	size_t size = NP_SIZE*8*4;
	int3 cube = {512, 512, 512};
	ocl_gltex3d(clVox, cube, GL_RGBA, GL_UNSIGNED_BYTE); //	1
	ocl_glbuf(clVox, sizeof(cl_float)*8, NULL);	// camera	2
	ocl_glbuf(clVox, size, NULL);	// NodeNode				3
	ocl_glbuf(clVox, size, NULL);	// NodeBrick			4
	ocl_glbuf(clVox, size, NULL);	// NodeReqTime			5
	ocl_glbuf(clVox, size, NULL);	// BrickReqTime			6
	ocl_glbuf(clVox, NP_SIZE*4, NULL);	// NodeUseTime		7
	ocl_glbuf(clVox, NP_SIZE*4, NULL);	// NodeLRU			8
	ocl_glbuf(clVox, NP_SIZE*4, NULL);	// NodeLRUOut		9
	ocl_glbuf(clVox, B_COUNT*4, NULL);	// BrickUseTime		10
	ocl_glbuf(clVox, B_COUNT*4, NULL);	// BrickLRU			11
	ocl_glbuf(clVox, B_COUNT*4, NULL);	// BrickLRUOut		12

//	voxel_ResetTime();

	if(!clVox->happy)printf("clVox is unhappy\n");


	s_Voxel = shader_load("data/Vertex.GLSL", "data/Voxel.GLSL");
	s_Brick = shader_load("data/Vertex.GLSL", "data/Brick.GLSL");
	s_BrickDry = shader_load("data/Vertex.GLSL", "data/BrickDry.GLSL");
	s_NodeClear = shader_load(0, "data/NodeClear.GLSL");
	s_NodeLRUReset = shader_load(0, "data/NodeLRUReset.GLSL");
	s_NodeLRUSort = shader_load(0, "data/NodeLRUSort.GLSL");
	s_BrickLRUReset = shader_load(0, "data/BrickLRUReset.GLSL");
	s_BrickLRUSort = shader_load(0, "data/BrickLRUSort.GLSL");
	s_NodeAlloc = shader_load(0, "data/NodeAlloc.GLSL");
	s_BrickAlloc = shader_load(0, "data/BrickAlloc.GLSL");

	
	shader_uniform(s_Voxel, "time");
//	shader_buffer(s_Voxel, "camera");
//	shader_buffer(s_Voxel, "node_node");
//	shader_buffer(s_Voxel, "node_brick");
//	shader_buffer(s_Voxel, "node_usetime");
//	shader_buffer(s_Voxel, "node_reqtime");
//	shader_buffer(s_Voxel, "brick_reqtime");
//	shader_buffer(s_Voxel, "brick_usetime");
	shader_uniform(s_Brick, "depth");
	shader_uniform(s_Brick, "bricks");
	shader_uniform(s_BrickDry, "time");
	shader_uniform(s_BrickDry, "depth");
	shader_uniform(s_NodeLRUReset, "time");
	shader_uniform(s_NodeLRUSort, "time");
	shader_uniform(s_BrickLRUReset, "time");
	shader_uniform(s_BrickLRUSort, "time");
	shader_uniform(s_NodeAlloc, "time");
	shader_uniform(s_BrickAlloc, "time");

	odd_frame = 0;
	voxel_NodeClear();
	voxel_NodeLRUReset(frame);
	voxel_BrickLRUReset(frame);
//	print_int(clVox->GLid[11]);

	glGenBuffers(1, &atomics);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomics);
	glBufferData(GL_ATOMIC_COUNTER_BUFFER,
			sizeof(GLuint)*2, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
}


extern float4 pos;
extern float4 angle;

float texdepth= 0;

void voxel_loop(void)
{
	if(!clVox)return;
	frame++;

	if(keys[KEY_L])
	{
		print_int(clVox->GLid[xBLU]);
		exit(0);
	}

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

	if(voxel_rebuildkernel_flag)
	{
		voxel_rebuildkernel_flag = 0;
		ocl_rebuild(clVox);
		if(clVox->happy)voxel_FindKernels();
	}

	if(voxel_rebuildshader_flag)
	{
		voxel_rebuildshader_flag = 0;
		shader_rebuild(s_Voxel);
		shader_rebuild(s_Brick);
	}

	// prepare the LRU table

	// create some bricks


	float tleft, tright;
	float ttop, tbottom;

	if(!use_glsl)
	{
		if(!clVox->happy)return;
		OCLPROGRAM *p = clVox;

		cl_int ret;
	//	cl_event e;
		size_t work_size[] = { vid_width, vid_height };

		glFinish();
		ocl_acquire(p);

		cl_kernel k = p->k[Knl_Render];
		ret = clSetKernelArg(k, 0, sizeof(cl_mem), &p->CLmem[0]);
		if(ret != CL_SUCCESS)printf("clSetKernelArg():%s\n", clStrError(ret));

		ret = clEnqueueWriteBuffer(OpenCL->q, p->CLmem[2], CL_TRUE, 0, 16,
			&pos, 0, NULL, NULL);
		if(ret != CL_SUCCESS)printf("clEnqueueWriteBuffer():%s\n",	clStrError(ret));
		ret = clEnqueueWriteBuffer(OpenCL->q, p->CLmem[2], CL_TRUE, 16, 16,
			&angle, 0, NULL, NULL);
		if(ret != CL_SUCCESS)printf("clEnqueueWriteBuffer():%s\n",	clStrError(ret));
	//	clEnqueueWriteBuffer(OpenCL->q, p->CLmem[2], CL_TRUE, 0, sizeof(float)*4, &angle, 0, NULL, NULL);
		ret = clSetKernelArg(k, 1, sizeof(cl_mem), &p->CLmem[1]);
		if(ret != CL_SUCCESS)printf("clSetKernelArg():%s\n", clStrError(ret));

		ret = clSetKernelArg(k, 2, sizeof(cl_mem), &p->CLmem[2]);
		if(ret != CL_SUCCESS)printf("clSetKernelArg():%s\n", clStrError(ret));
		ret = clSetKernelArg(k, 3, sizeof(float), &time);
		if(ret != CL_SUCCESS)printf("clSetKernelArg():%s\n", clStrError(ret));
		clSetKernelArg(k, 4, sizeof(cl_mem), &p->CLmem[3]);
		clSetKernelArg(k, 5, sizeof(cl_mem), &p->CLmem[4]);
		clSetKernelArg(k, 6, sizeof(cl_mem), &p->CLmem[5]);
		clSetKernelArg(k, 7, sizeof(cl_mem), &p->CLmem[6]);
		clSetKernelArg(k, 8, sizeof(cl_mem), &p->CLmem[7]);
		clSetKernelArg(k, 9, sizeof(cl_mem), &p->CLmem[8]);

		ret = clEnqueueNDRangeKernel(OpenCL->q, k, 2,NULL, work_size,
				NULL, 0, NULL, NULL);
		if(ret != CL_SUCCESS)
		{
			p->happy = 0;
			printf("clEnqueueNDRangeKernel():%s\n", clStrError(ret));
		}

		ocl_release(p);
		clFinish(OpenCL->q);
	//	clWaitForEvents(1, &e);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, p->GLid[0]);

		tleft = ttop = 0;
		tright = (float)(vid_width)/(float)sys_width;
		tbottom = (float)(vid_height)/(float)sys_height;
	}
	else
	{

		voxel_Voxel(frame);
//		glBindBuffer(GL_ARRAY_BUFFER, node_pool);

		tleft = 0; tright = 1;
		float srat = ((float)vid_height / (float)vid_width) * 0.5;
		ttop = 0.5 - srat;
		tbottom = 0.5 + srat;
	}

	float left = 0, right = vid_width;
	float top = 0, bottom = vid_height;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, vid_width, 0, vid_height, -1000, 5000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glColor4f(1,1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(tleft, ttop); glVertex2f(left, top);
	glTexCoord2f(tright, ttop); glVertex2f(right, top);
	glTexCoord2f(tright, tbottom); glVertex2f(right, bottom);
	glTexCoord2f(tleft, tbottom); glVertex2f(left, bottom);
	glEnd();



	if(!use_glsl)
	{
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
	}
	else
	{
		glUseProgram(0);
	}

	int depth = 4;


//	glPushMatrix();
//	glMatrixMode(GL_PROJECTION);
//	glPushMatrix();
//	glLoadIdentity();
//	glMatrixMode(GL_MODELVIEW);
//	glLoadIdentity();
//	glClearDepth(5000.0f);
//	glClear(GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	float scale = 500;	
	glScalef(scale, scale, scale);
//	if(vobj)vobj->draw(vobj);
/*
	ocl_gltex3d(clVox, cube, GL_RGBA, GL_UNSIGNED_BYTE); //	1
	ocl_glbuf(clVox, sizeof(cl_float)*8, NULL);	// camera	2
	ocl_glbuf(clVox, size, NULL);	// NodeNode				3
	ocl_glbuf(clVox, size, NULL);	// NodeBrick			4
	ocl_glbuf(clVox, size, NULL);	// NodeReqTime			5
	ocl_glbuf(clVox, size, NULL);	// BrickReqTime			6
	ocl_glbuf(clVox, NP_SIZE*4, NULL);	// NodeUseTime		7
	ocl_glbuf(clVox, NP_SIZE*4, NULL);	// NodeLRU			8
	ocl_glbuf(clVox, NP_SIZE*4, NULL);	// NodeLRUOut		9
	ocl_glbuf(clVox, B_COUNT*4, NULL);	// BrickUseTime		10
	ocl_glbuf(clVox, B_COUNT*4, NULL);	// BrickLRU			11
	ocl_glbuf(clVox, B_COUNT*4, NULL);	// BrickLRUOut		12
*/

	voxel_NodeLRUSort(frame);
	voxel_BrickLRUSort(frame);
	odd_frame = !odd_frame;
	frame++;
	voxel_BrickDry(frame, depth);
	// render
	if(vobj)vobj->draw(vobj);

//	printf("rend glerror=%s\n", glError(glGetError()));

	voxel_NodeAlloc(frame);
//	printf("na glerror=%s\n", glError(glGetError()));
	voxel_BrickAlloc(frame);
//	glMemoryBarrier(GL_ALL_BARRIER_BITS);
//	printf("ba glerror=%s\n", glError(glGetError()));
	voxel_Brick(frame, depth);
//	printf("b glerror=%s\n", glError(glGetError()));

	// render
	if(vobj)vobj->draw(vobj);


	glUseProgram(0);
//	glMatrixMode(GL_PROJECTION);
//	glPopMatrix();
//	glMatrixMode(GL_MODELVIEW);
//	glPopMatrix();
	glDisable(GL_DEPTH_TEST);

	glEnable(GL_TEXTURE_3D);
	glBindTexture(GL_TEXTURE_3D, clVox->GLid[1]);
	
	float toff = 0.5 / 512.0;

	glBegin(GL_QUADS);
	glTexCoord3f(0, 0, texdepth + toff); glVertex2f(1, 0);
	glTexCoord3f(1, 0, texdepth+toff); glVertex2f(2, 0);
	glTexCoord3f(1, 1, texdepth+toff); glVertex2f(2, 1);
	glTexCoord3f(0, 1, texdepth+toff); glVertex2f(1, 1);
	glEnd();

	glDisable(GL_TEXTURE_3D);
	glBindTexture(GL_TEXTURE_3D, 0);
}

void voxel_end(void)
{

	ocl_free(clVox);

}


void voxel_open(char *filename)
{



}


