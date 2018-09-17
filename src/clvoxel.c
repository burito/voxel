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
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenCL/cl.h>
#else
#include <GL/glew.h>

#endif

#include "log.h"

#include <stdio.h>
#include <string.h>

#include "ocl.h"
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


#define FBUFFER_SIZE	4096
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

void voxel_rebuildkernel(widget* foo)
{
	voxel_rebuildkernel_flag = 1;
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
//	log_trace("Error = \"%s\"", glError(glGetError()));
	glBindTexture(GL_TEXTURE_3D, 0);
	ocl_gltex3d(clVox, id);
	return id;
}

GLuint vox_glbuf(int size, void *ptr)
{
	GLuint buf;
	glGenBuffers(1, &buf);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, ptr, GL_DYNAMIC_COPY);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	ocl_glbuf(clVox, buf);
	return buf;
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


static void clvox_ResetTime(void)
{
	// Tell the GPU to reset the time buffers
	if(!clVox->happy)return;
	glFinish();
	ocl_acquire(clVox);
	cl_kernel k = clVox->k[Knl_ResetNodeTime];
	clSetKernelArg(k, 0, sizeof(float), &time);
	clSetKernelArg(k, 1, sizeof(cl_mem), &clVox->CLmem[9]);
	clSetKernelArg(k, 2, sizeof(cl_mem), &clVox->CLmem[7]);
	clSetKernelArg(k, 3, sizeof(cl_mem), &clVox->CLmem[8]);

	size_t work_size = np_size;
	cl_int ret = clEnqueueNDRangeKernel(OpenCL->q, k, 1,NULL, &work_size,
			NULL, 0, NULL, NULL);
	if(ret != CL_SUCCESS)
	{
		clVox->happy = 0;
		log_fatal("clEnqueueNDRangeKernel() = %s", clStrError(ret));
	}

	// Tell the GPU to reset the brick time
	k = clVox->k[Knl_ResetBrickTime];
	clSetKernelArg(k, 0, sizeof(float), &time);
	clSetKernelArg(k, 1, sizeof(cl_mem), &clVox->CLmem[12]);
	clSetKernelArg(k, 2, sizeof(cl_mem), &clVox->CLmem[13]);

	work_size = B_COUNT;
	ret = clEnqueueNDRangeKernel(OpenCL->q, k, 1,NULL, &work_size,
			NULL, 0, NULL, NULL);
	if(ret != CL_SUCCESS)
	{
		clVox->happy = 0;
		log_fatal("clEnqueueNDRangeKernel() = %s", clStrError(ret));
	}
	ocl_release(clVox);
}



float texdepth= 0;
void voxel_3dtexdraw(void)
{
	if(keys[KEY_M])
	{
		if(texdepth < 511.0/512.0) texdepth += 1.0 / 512;
		log_trace("%d", (int)(texdepth*512.0));
	}
	if(keys[KEY_N])
	{
		if(texdepth > 0.0) texdepth -= 1.0 / 512;
		log_trace("%d", (int)(texdepth*512.0));
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

	extern vec4 pos, angle;

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
	glBindImageTexture(4, t3DBrick, 0, /*layered=*/GL_TRUE, 0,
	GL_READ_ONLY, GL_RGBA16F);
	glUniform1i(s_Voxel->unif[1], 4);

	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_3D, t3DBrickColour);

	glBindImageTexture(5, t3DBrickColour, 0, /*layered=*/GL_TRUE, 0,
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
	log_debug("Allocated b=%d, n=%d", countb, countn);
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

	clVox = ocl_build("./data/shaders/Voxel.OpenCL");
	ocl_rm(clVox);
	if(clVox->happy)voxel_FindKernels();

	size_t size = np_size*8*4;

	int b_edge = 64;
	int b_size = 8;

	log_info("Total VRAM : %dk", total_vram);

	if(total_vram > 1200000) // 1.2gb of vram?
	{	// defaults
		b_size = 8;
		b_edge = 64;
		np_size = 100000;
	}
	else
	{	// works on 2Gb VRAM
		b_size = 8;
		b_edge = 48;
		np_size = 100000;
	}

// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shading_language_include.txt

	char include_name[] = "/voxel_data";
	char include_string[1000];
	sprintf(include_string,
		"#define B_SIZE %d\n"
		"#define B_EDGE %d\n"
		"#define NP_SIZE %d\n",
		b_size, b_edge, np_size );

	glNamedStringARB(GL_SHADER_INCLUDE_ARB,
		strlen(include_name), include_name,
		strlen(include_string), include_string );

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
//	glTexImage2D(GL_TEXTURE_2D, 0, intfmt, FBUFFER_SIZE, FBUFFER_SIZE, 0, type, GL_UNSIGNED_BYTE, img->buf);
//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbtex, 0);
	glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, FBUFFER_SIZE);
	glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, FBUFFER_SIZE);
	glFramebufferParameteri(GL_FRAMEBUFFER,GL_FRAMEBUFFER_DEFAULT_SAMPLES, 0);
	glFramebufferParameteri(GL_FRAMEBUFFER,GL_FRAMEBUFFER_DEFAULT_LAYERS, 0);
	GLenum err;
	err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(err != GL_FRAMEBUFFER_COMPLETE)
	{
		log_error("Creating Empty Framebuffer failed %s", glError(err));

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

	log_debug("Atomics %d, %d", atm[0], atm[1]);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
}

int2 atm;
int2 get_atoms(void)
{
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomics);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, atomic_read);

	glCopyBufferSubData(GL_ATOMIC_COUNTER_BUFFER,
			GL_SHADER_STORAGE_BUFFER, 0, 0, 8);

	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 8, (GLuint*)&atm);

//	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
//	log_error("glUnmapBuffer(ssb)-%s", glError(glGetError()));
//	glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
//	log_Error("glUnmapBuffer(atomic)-%s", glError(glGetError()));

	return atm;
}


extern vec4 pos;
extern vec4 angle;


void voxel_loop(void)
{
	if(!clVox)return;
	frame++;

	if(keys[KEY_V])
	{
		keys[KEY_V] = 0;
		if(tree_depth > 1)tree_depth--;
		log_verbose("tree depth=%d", tree_depth);
	}
	if(keys[KEY_B])
	{
		keys[KEY_B] = 0;
		if(tree_depth < 20)tree_depth++;
		log_verbose("tree depth=%d", tree_depth);
	}

	if(keys[KEY_Z])
	{
		keys[KEY_Z] = 0;
		voxel_rebuildshader_flag = 1;
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

	/* Render the oct-tree to screen */
	if(!use_glsl)
	{	/* OpenCL Rendering */
		if(!clVox->happy)return;
		OCLPROGRAM *p = clVox;

		cl_int ret;
	//	cl_event e;
		size_t work_size[] = { vid_width, vid_height };

		glFinish();
		ocl_acquire(p);
/*	1 = t3DBrick = vox_3Dtex(cube, GL_RGBA16F, GL_LINEAR);
	2 = t3DBrickColour = vox_3Dtex(cube, GL_RGBA8, GL_LINEAR);
	3 = bCamera = vox_glbuf(sizeof(float)*8, NULL);	// camera
	4 = bNN = vox_glbuf(size, NULL);	// NodeNode
	5 = bNL = vox_glbuf(size, NULL);	// NodeLocality
	6 = bNB = vox_glbuf(size, NULL);	// NodeBrick
	7 = bNRT = vox_glbuf(size, NULL);	// NodeReqTime
	8 = bBRT = vox_glbuf(size, NULL);	// BrickReqTime
	9 = bNUT = vox_glbuf(np_size*4, NULL);	// NodeUseTime
	10 = bNLU = vox_glbuf(np_size*4, NULL);	// NodeLRU
	11 = bNLO = vox_glbuf(np_size*4, NULL);	// NodeLRUOut
	12 = bBUT = vox_glbuf(B_COUNT*4, NULL);	// BrickUseTime
	13 = bBLU = vox_glbuf(B_COUNT*4, NULL);	// BrickLRU
	14 = bBLO = vox_glbuf(B_COUNT*4, NULL);	// BrickLRUOut
	15 = bBL = vox_glbuf(B_COUNT*4, NULL);	// BrickLocality

	//OpenCL order...
	0 = output buffer
	1 = brick normal
	2 = brick colour
	3 = camera pos
	4 = time
	5 = node node	= 4
	6 = node brick	= 6
	7 = nodeRU	= 7
	8 = bRU		= 8
	9 = nUT		= 9
	10 = bUT	= 12

*/
		cl_kernel k = p->k[Knl_Render];
		ret = clSetKernelArg(k, 0, sizeof(cl_mem), &p->CLmem[0]);
		if(ret != CL_SUCCESS)log_fatal("clSetKernelArg():%s", clStrError(ret));

		ret = clEnqueueWriteBuffer(OpenCL->q, p->CLmem[3], CL_TRUE, 0, 16,
			&pos, 0, NULL, NULL);
		if(ret != CL_SUCCESS)log_fatal("clEnqueueWriteBuffer():%s",	clStrError(ret));
		ret = clEnqueueWriteBuffer(OpenCL->q, p->CLmem[3], CL_TRUE, 16, 16,
			&angle, 0, NULL, NULL);
		if(ret != CL_SUCCESS)log_fatal("clEnqueueWriteBuffer():%s",	clStrError(ret));
		clEnqueueWriteBuffer(OpenCL->q, p->CLmem[2], CL_TRUE, 0, sizeof(float)*4, &angle, 0, NULL, NULL);

		ret = clSetKernelArg(k, 1, sizeof(cl_mem), &p->CLmem[1]);	// brick buffer
		if(ret != CL_SUCCESS)log_fatal("clSetKernelArg():%s", clStrError(ret));
		ret = clSetKernelArg(k, 2, sizeof(cl_mem), &p->CLmem[2]);	// brick colour buffer
		if(ret != CL_SUCCESS)log_fatal("clSetKernelArg():%s", clStrError(ret));

		ret = clSetKernelArg(k, 3, sizeof(cl_mem), &p->CLmem[3]);	// camera position buffer
		if(ret != CL_SUCCESS)log_fatal("clSetKernelArg():%s", clStrError(ret));

		ret = clSetKernelArg(k, 4, sizeof(GLint), &frame);		// time
		if(ret != CL_SUCCESS)log_fatal("clSetKernelArg():%s", clStrError(ret));
		clSetKernelArg(k, 5, sizeof(cl_mem), &p->CLmem[4]);
		clSetKernelArg(k, 6, sizeof(cl_mem), &p->CLmem[6]);
		clSetKernelArg(k, 7, sizeof(cl_mem), &p->CLmem[7]);
		clSetKernelArg(k, 8, sizeof(cl_mem), &p->CLmem[8]);
		clSetKernelArg(k, 9, sizeof(cl_mem), &p->CLmem[9]);
		clSetKernelArg(k, 10, sizeof(cl_mem), &p->CLmem[12]);


		ret = clEnqueueNDRangeKernel(OpenCL->q, k, 2,NULL, work_size,
				NULL, 0, NULL, NULL);
		if(ret != CL_SUCCESS)
		{
			p->happy = 0;
			log_fatal("clEnqueueNDRangeKernel():%s", clStrError(ret));
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
	{	/* GLSL Rendering */
		voxel_Voxel(frame);

		tleft = 0; tright = 1;
		float srat = ((float)vid_height / (float)vid_width) * 0.5;
		ttop = 0.5 - srat;
		tbottom = 0.5 + srat;
	}

	/* Output the Quad that covers the screen */
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

	if(!use_glsl)
	{
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
	}
	else
	{
		glUseProgram(0);
	}


	/* build the oct-tree */

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
//		float scale = FBUFFER_SIZE;
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
//	log_trace("Nodes = %d, Bricks = %d", used_nodes, used_bricks);

	if(texdraw)voxel_3dtexdraw();

}

void voxel_end(void)
{
	ocl_free(clVox);

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
//	if(use_glsl)
//	{
		voxel_NodeClear();
		voxel_NodeLRUReset(frame);
		voxel_BrickLRUReset(frame);
//	}
//	else
//	{
//		clvox_ResetTime();
//	}
}


