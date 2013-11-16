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

#define NODE_COUNT 100000
#define BRICK_SIZE 8
#define BRICK_EDGE 64
#define BRICK_COUNT (BRICK_EDGE*BRICK_EDGE*BRICK_EDGE)

OCLPROGRAM *clVox=NULL;
int voxel_rebuild_flag=0;
int Knl_Render;
int Knl_ResetNodeTime;
int Knl_ResetBrickTime;

void voxel_rebuild(widget* foo)
{
	voxel_rebuild_flag = 1;
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
	}
}

static void voxel_ResetTime(void)
{
	// Tell the GPU to reset the time buffers
	cl_kernel k = clVox->k[Knl_ResetNodeTime];
	clSetKernelArg(k, 0, sizeof(float), &time);
	clSetKernelArg(k, 1, sizeof(cl_mem), &clVox->CLmem[3]);
	clSetKernelArg(k, 2, sizeof(cl_mem), &clVox->CLmem[4]);
	clSetKernelArg(k, 3, sizeof(cl_mem), &clVox->CLmem[5]);

	size_t work_size = NODE_COUNT;
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

	work_size = BRICK_COUNT;
	ret = clEnqueueNDRangeKernel(OpenCL->q, k, 1,NULL, &work_size,
			NULL, 0, NULL, NULL);
	if(ret != CL_SUCCESS)
	{
		clVox->happy = 0;
		printf("clEnqueueNDRangeKernel():%s\n", clStrError(ret));
	}
}


void voxel_init(void)
{
	clVox = ocl_build("./data/Voxel.OpenCL");
	ocl_rm(clVox);
	if(clVox->happy)voxel_FindKernels();
	
	clVox->num_glid = 2;
	clVox->num_clmem = 9;

	void* tmp;
	tmp = realloc(clVox->GLid, sizeof(GLuint)*clVox->num_glid);
	if(!tmp){printf("voxel_init():realloc(GLid) fail\n");return;}
	clVox->GLid = tmp;

	tmp = realloc(clVox->CLmem, sizeof(cl_mem)*clVox->num_clmem);
	if(!tmp){printf("voxel_init():realloc(CLmem) fail\n");return;}
	clVox->CLmem = tmp;
	
	cl_int ret;
	GLuint id;
	// create GL brick texture
	glGenTextures(1, &id);
	clVox->GLid[1] = id;
	glBindTexture(GL_TEXTURE_3D, id);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 512, 512, 512, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_3D, 0);

	clVox->CLmem[1] = clCreateFromGLTexture3D(OpenCL->c, CL_MEM_READ_ONLY,
		GL_TEXTURE_3D, 0, id, &ret);
	if(ret != CL_SUCCESS)
	{
		printf("clCreateFromGLTexture3D():%s\n", clStrError(ret));
	}

	// And the buffers... NodeNode, NodeBrick,
	// NodeUseTime, NodeReqTime, BrickReqTime, BrickUseTime
	size_t size = NODE_COUNT*8*4;
	clVox->CLmem[2] = clCreateBuffer(OpenCL->c, CL_MEM_READ_ONLY,
		sizeof(cl_float)*8, NULL, &ret);	// camera
	if(ret != CL_SUCCESS)printf("clvox:buffer 2:%s\n", clStrError(ret));
	clVox->CLmem[3] = clCreateBuffer(OpenCL->c, CL_MEM_READ_WRITE,
		size, 0, &ret);		// NodeNode
	if(ret != CL_SUCCESS)printf("clvox:buffer 4:%s\n", clStrError(ret));
	clVox->CLmem[4] = clCreateBuffer(OpenCL->c, CL_MEM_READ_WRITE, 
		size,0,&ret);		// NodeBrick
	if(ret != CL_SUCCESS)printf("clvox:buffer 5:%s\n", clStrError(ret));
	clVox->CLmem[5] = clCreateBuffer(OpenCL->c, CL_MEM_READ_WRITE,
		size,0,&ret);		// NodeUseTime
	if(ret != CL_SUCCESS)printf("clvox:buffer 6:%s\n", clStrError(ret));
	clVox->CLmem[6] = clCreateBuffer(OpenCL->c, CL_MEM_READ_WRITE,
		size,0,&ret);		// NodeReqTime
	if(ret != CL_SUCCESS)printf("clvox:buffer 7:%s\n", clStrError(ret));
	clVox->CLmem[7] = clCreateBuffer(OpenCL->c, CL_MEM_READ_WRITE,
		size,0,&ret);		// BrickReqTime
	if(ret != CL_SUCCESS)printf("clvox:buffer 8:%s\n", clStrError(ret));
	clVox->CLmem[8] = clCreateBuffer(OpenCL->c, CL_MEM_READ_WRITE,
		BRICK_COUNT * sizeof(float),0,&ret);		// BrickUseTime
	if(ret != CL_SUCCESS)printf("clvox:buffer 9:%s\n", clStrError(ret));

	voxel_ResetTime();

	if(clVox->happy)printf("everything is happy\n");
}

extern float4 pos;
extern float4 angle;


void voxel_loop(void)
{
	if(!clVox)return;
	if(voxel_rebuild_flag)
	{
		voxel_rebuild_flag = 0;
		ocl_rebuild(clVox);
		if(clVox->happy)voxel_FindKernels();
	}

	if(!clVox->happy)return;
	OCLPROGRAM *p = clVox;

	cl_int ret;
//	cl_event e;
	size_t work_size[] = { vid_width, vid_height };

	glFinish();
	ret = clEnqueueAcquireGLObjects(OpenCL->q, 1, &p->CLmem[0], 0, 0, 0);
	if(ret != CL_SUCCESS)printf("clEnqueueAcquireGLObjects():%s\n",	clStrError(ret));

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
	// get brick texture
	ret = clEnqueueAcquireGLObjects(OpenCL->q, 1, &p->CLmem[1], 0, 0, 0);
	if(ret != CL_SUCCESS)printf("clEnqueueAcquireGLObjects():%s\n",	clStrError(ret));
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


	ret = clEnqueueReleaseGLObjects(OpenCL->q, 1, &p->CLmem[0], 0, 0, 0);
	if(ret != CL_SUCCESS)printf("clEnqueueReleaseGLObjects():%s\n", clStrError(ret));

	ret = clEnqueueReleaseGLObjects(OpenCL->q, 1, &p->CLmem[1], 0, 0, 0);
	if(ret != CL_SUCCESS)printf("clEnqueueReleaseGLObjects():%s\n", clStrError(ret));

	clFinish(OpenCL->q);
//	clWaitForEvents(1, &e);


	float tx = (float)(vid_width)/(float)sys_width;
	float ty = (float)(vid_height)/(float)sys_height;
	
	float left = 0, right = vid_width;
	float top = 0, bottom = vid_height;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, vid_width, 0, vid_height, -1, 5000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glColor4f(1,1,1,1);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, p->GLid[0]);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(left, top);
	glTexCoord2f(tx, 0); glVertex2f(right, top);
	glTexCoord2f(tx, ty); glVertex2f(right, bottom);
	glTexCoord2f(0, ty); glVertex2f(left, bottom);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
}

void voxel_end(void)
{

	ocl_free(clVox);

}


void voxel_open(char *filename)
{



}


