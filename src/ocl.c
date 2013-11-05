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

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <GL/glx.h>
#endif

#define _MSC_VER 1
#include <CL/cl_gl.h>

#include <CL/cl.h>

#include "ocl.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "3dmaths.h"



char* loadTextFile(char *filename)
{
	FILE *fptr = fopen(filename, "rb");
	if(!fptr)return 0;
	struct stat stbuf;
	stat(filename, &stbuf);
	int size = stbuf.st_size;
	char *buf = malloc(size+1);
	buf[size] = 0;
	fread(buf, size, 1, fptr);
	fclose(fptr);
	return buf;
}


OCLCONTEXT *OpenCL;

void ocl_end(void)
{
	clReleaseCommandQueue(OpenCL->q);
	clReleaseContext(OpenCL->c);
	for(int i=0; i < OpenCL->num_pid; i++)
		if(OpenCL->num_did[i])
			free(OpenCL->did[i]);
	free(OpenCL->did);
	free(OpenCL->num_did);
	free(OpenCL->pid);
	free(OpenCL);
}
int ocl_init(void)
{
	cl_int ret;
	OCLCONTEXT *c = malloc(sizeof(OCLCONTEXT));
	memset(c, 0, sizeof(OCLCONTEXT));
	OpenCL = c;

	// get OpenCL Platform ID info
	clGetPlatformIDs(0, 0, &c->num_pid);
	c->pid = malloc(sizeof(cl_platform_id) * c->num_pid);
	memset(c->pid, 0, sizeof(cl_platform_id) * c->num_pid);
	clGetPlatformIDs(c->num_pid, c->pid, &c->num_pid);

	// query those PIDs for Devices
	c->num_did = malloc(sizeof(cl_uint*)*c->num_pid);
	memset(c->num_did, 0, sizeof(cl_uint*)*c->num_pid);
	c->did = malloc(sizeof(cl_device_id*)*c->num_pid);
	memset(c->did, 0, sizeof(cl_device_id*)*c->num_pid);
	for(int i=0; i<c->num_pid; i++)
	{
		clGetDeviceIDs(c->pid[i], CL_DEVICE_TYPE_ALL, 0, 0, &c->num_did[i]);
		c->did[i] = malloc(sizeof(cl_device_id)*c->num_did[i]);
		memset(c->did[i], 0, sizeof(cl_device_id)*c->num_did[i]);
		clGetDeviceIDs(c->pid[i], CL_DEVICE_TYPE_ALL, c->num_did[i],
				c->did[i], &c->num_did[i]);

		char b[1024];
		size_t bs = 1024;
		clGetPlatformInfo( c->pid[i], CL_PLATFORM_NAME, bs, b, &bs);
		printf("cl->pid[%d]:\"%s\"\n", i, b);
		for(int j=0; j < c->num_did[i]; j++)
		{
			bs = 1024;
			clGetDeviceInfo(c->did[i][j], CL_DEVICE_EXTENSIONS, bs, b, &bs);
			char *supported = strstr(b, "cl_khr_gl_sharing");
			printf("cl->did[%d][%d]%s:", i, j, (supported?":GL":":-("));
			bs = 1024;
			clGetDeviceInfo(c->did[i][j], CL_DEVICE_NAME, bs, b, &bs);
			printf("\"%s\"\n", b);
		}
	}

	// select a platform and device to use
	c->p = &c->pid[0]; c->d = &c->did[0][0];

	// create the context
	cl_context_properties properties[] = {
#ifdef WIN32
		CL_GL_CONTEXT_KHR,		(cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR,			(cl_context_properties)wglGetCurrentDC(),
#else
		CL_GL_CONTEXT_KHR,		(cl_context_properties)glXGetCurrentContext(),
		CL_GLX_DISPLAY_KHR,		(cl_context_properties)glXGetCurrentDisplay(),
#endif
		CL_CONTEXT_PLATFORM,	(cl_context_properties)*c->p,
		0};
	c->c = clCreateContext(properties, 1, c->d, NULL, NULL, &ret);
	if(ret != CL_SUCCESS)
	{
		printf("failed: clCreateContext() = %d\n", ret);
		return 1;
	}

	// create a command queue
	c->q = clCreateCommandQueue(c->c, *c->d, 0, &ret);
	if(ret != CL_SUCCESS)
	{
		printf("failed: clCreateCommandQueue() = %d\n", ret);
		return 2;
	}
	OpenCL->happy = 1;
	return 0;
}


void ocl_free(OCLPROGRAM *p)
{
	if(!p)return;
	for(int i=0; i<p->num_kernels; i++)
		clReleaseKernel(p->k[i]);
	free(p->k);
	clReleaseProgram(p->pr);

	glDeleteTextures(p->num_glid, p->GLid);
	free(p->GLid);

	for(int i=0; i<p->num_clmem; i++)
		clReleaseMemObject(p->CLmem[i]);
	free(p->CLmem);
}

void ocl_add(OCLPROGRAM *p)
{
	if(!OpenCL)return;
	if(!p)return;

	OpenCL->num_progs++;

	void *tmp = realloc(OpenCL->progs, sizeof(OCLPROGRAM)*OpenCL->num_progs);

	if(!tmp)
	{
		printf("ocl add failed\n");
		return;
	}

	OpenCL->progs = tmp;
	OpenCL->progs[OpenCL->num_progs-1] = *p;
}


OCLPROGRAM* ocl_build(char *filename)
{
	if(!OpenCL->happy)return 0;

	OCLPROGRAM *clprog = malloc(sizeof(OCLPROGRAM));
	memset(clprog, 0, sizeof(OCLPROGRAM));

	const char *src = loadTextFile(filename);
	size_t size = strlen(src);
	cl_int ret;
	cl_program p = clCreateProgramWithSource(OpenCL->c, 1, &src, &size, &ret);
	if(ret != CL_SUCCESS){printf("cl_build:createprog %d\n", ret);return 0;}
	ret = clBuildProgram(p, 1, OpenCL->d, NULL, NULL, NULL);

	if(ret != CL_SUCCESS)
	{
		int esize = 100000;
		char *error = malloc(esize);
		memset(error, 0, esize);
		clGetProgramBuildInfo(p, *OpenCL->d, CL_PROGRAM_BUILD_LOG, esize,error,NULL);
		printf("clBuildProgram(\"%s\")%d\n%s\n", filename, ret, error);
		return 0;
	}

	clGetProgramInfo(p, CL_PROGRAM_BINARY_SIZES,
				sizeof(size), &size, NULL);
//	printf("Success: %zu bytes.\n", size);

	cl_uint nk=0;
	clCreateKernelsInProgram(p, 0, NULL, &nk);
	if(!nk)
	{
		printf("no kernels found\n");
		return 0;
	}

	cl_kernel *k = malloc(sizeof(cl_kernel)*nk);
	memset(k, 0, sizeof(cl_kernel)*nk);
	ret = clCreateKernelsInProgram(p, nk, k, NULL);

	int voxel=0;

	for(int i=0; i<nk; i++)
	{
		char buf[256];
		memset(buf, 0, 256);
		clGetKernelInfo(k[i], CL_KERNEL_FUNCTION_NAME, 255, buf, NULL);
		if(strstr("voxel", buf))
			voxel = 1;
		cl_uint argc = 0;
		clGetKernelInfo(k[i], CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &argc, NULL);
		printf("Kernel[%d]:%s(%d)\n", i, buf, argc);
	}

	clprog->pr = p;
	clprog->k = k;
	clprog->num_kernels = nk;

	// create GL texture
	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	cl_mem output = clCreateFromGLTexture2D(OpenCL->c, CL_MEM_WRITE_ONLY,
		GL_TEXTURE_2D, 0, id, &ret);
	if(ret != CL_SUCCESS)
	{
		printf("clCreateFromGLTexture2D() failed\n");
	}
	
	clprog->num_glid = 1;
	clprog->num_clmem = 1;

	if(voxel)
	{
		clprog->type = 1;
		clprog->num_glid = 2;
		clprog->num_clmem = 7;
	}

	clprog->GLid = malloc(sizeof(GLuint)*clprog->num_glid);
	clprog->CLmem = malloc(sizeof(cl_mem)*clprog->num_clmem);
	clprog->GLid[0] = id;
	clprog->CLmem[0] = output;

	if(voxel)
	{
		// create GL brick texture
		cl_int err;
		glGenTextures(1, &id);
		clprog->GLid[1] = id;
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

		clprog->CLmem[1] = clCreateFromGLTexture3D(OpenCL->c, CL_MEM_READ_ONLY,
			GL_TEXTURE_3D, 0, id, &err);
		if(ret != CL_SUCCESS)
		{
			printf("clCreateFromGLTexture3D() failed\n");
		}

		// And the buffers... NodeNode, NodeBrick,
		// NodeUseTime, NodeReqTime, BrickUseTime
		size_t size = 100000*4;
		clprog->CLmem[2] = clCreateBuffer(OpenCL->c,CL_MEM_READ_WRITE,size,0,&err);
		if(err != CL_SUCCESS)printf("clvox:buffer 2 failed\n");
		clprog->CLmem[3] = clCreateBuffer(OpenCL->c,CL_MEM_READ_WRITE,size,0,&err);
		if(err != CL_SUCCESS)printf("clvox:buffer 3 failed\n");
		clprog->CLmem[4] = clCreateBuffer(OpenCL->c,CL_MEM_READ_WRITE,size,0,&err);
		if(err != CL_SUCCESS)printf("clvox:buffer 4 failed\n");
		clprog->CLmem[5] = clCreateBuffer(OpenCL->c,CL_MEM_READ_WRITE,size,0,&err);
		if(err != CL_SUCCESS)printf("clvox:buffer 5 failed\n");
		clprog->CLmem[6] = clCreateBuffer(OpenCL->c,CL_MEM_READ_WRITE,size,0,&err);
		if(err != CL_SUCCESS)printf("clvox:buffer 6 failed\n");
	}

	ocl_add(clprog);
	return clprog;
}



void ocl_loop(void)
{
	if(!OpenCL)return;
	if(!OpenCL->happy)return;

	float3 position = {0,0,0};
	float3 angle = {0,0,0};

	for(int i=0; i<OpenCL->num_progs; i++)
	{
		OCLPROGRAM *p = &OpenCL->progs[i];

		cl_int ret;
//	cl_event e;
		size_t work_size[] = { 1024, 1024 };

		glFinish();
		ret = clEnqueueAcquireGLObjects(OpenCL->q, 1, &p->CLmem[0], 0, 0, 0);
		if(ret != CL_SUCCESS)printf("clEnqueueAcquireGLObjects():%d\n", ret);

		cl_kernel k = OpenCL->progs[i].k[0];
		clSetKernelArg(k, 0, sizeof(cl_mem), &p->CLmem[0]);
		clSetKernelArg(k, 1, sizeof(float), &time);

		if(p->type)
		{
			clSetKernelArg(k, 2, sizeof(cl_mem), &p->CLmem[1]);
			clSetKernelArg(k, 3, sizeof(cl_mem), &p->CLmem[2]);
			clSetKernelArg(k, 4, sizeof(cl_mem), &p->CLmem[3]);
			clSetKernelArg(k, 5, sizeof(cl_mem), &p->CLmem[4]);
			clSetKernelArg(k, 6, sizeof(cl_mem), &p->CLmem[5]);
			clSetKernelArg(k, 7, sizeof(cl_mem), &p->CLmem[6]);
			clSetKernelArg(k, 8, sizeof(float)*3, &position);
			clSetKernelArg(k, 9, sizeof(float)*3, &angle);
		}

		ret = clEnqueueNDRangeKernel(OpenCL->q, k, 2,NULL, work_size, NULL, 0, NULL, 0);
		if(ret != CL_SUCCESS)printf("clEnqueueNDRangeKernel():%d\n", ret);


		ret = clEnqueueReleaseGLObjects(OpenCL->q, 1, &p->CLmem[0], 0, 0, 0);
		if(ret != CL_SUCCESS)printf("clEnqueueReleaseGLObjects():%d\n", ret);
		clFinish(OpenCL->q);
	//	clWaitForEvents(1, &e);
	}

}


