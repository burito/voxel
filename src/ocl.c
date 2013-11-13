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

#include "3dmaths.h"
#include "gui.h"
#include "text.h"


OCLCONTEXT *OpenCL;
static char* clStrError(cl_int error);

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
		printf("clCreateContext():%s\n", clStrError(ret));
		return 1;
	}

	// create a command queue
	c->q = clCreateCommandQueue(c->c, *c->d, 0, &ret);
	if(ret != CL_SUCCESS)
	{
		printf("failed: clCreateCommandQueue() = %s\n", clStrError(ret));
		return 2;
	}
	OpenCL->happy = 1;
	return 0;
}



void ocl_add(OCLPROGRAM *p)
{
	if(!OpenCL)return;
	if(!p)return;

	// if there's an empty spot in our list
	if(OpenCL->num_progs > 0)
	for(int i=0; i<OpenCL->num_progs; i++)
	if(!OpenCL->progs[i])
	{
		OpenCL->progs[i] = p;
		return;
	}
	
	// otherwise our list must grow
	OpenCL->num_progs++;
	void *tmp = realloc(OpenCL->progs, sizeof(OCLPROGRAM*)*OpenCL->num_progs);
	if(!tmp)
	{
		printf("ocl add failed\n");
		return;
	}
	OpenCL->progs = tmp;
	OpenCL->progs[OpenCL->num_progs-1] = p;
}

void ocl_rm(OCLPROGRAM *p)
{
	for(int i=0; i<OpenCL->num_progs; i++)
	{
		if(OpenCL->progs[i] == p)
			OpenCL->progs[i] = NULL;
	}
}

void ocl_free(OCLPROGRAM *p)
{
	if(!p)return;

	ocl_rm(p);

	if(p->k)
	{
		for(int i=0; i<p->num_kernels; i++)
			clReleaseKernel(p->k[i]);
		free(p->k);
	}
	clReleaseProgram(p->pr);

	glDeleteTextures(p->num_glid, p->GLid);
	free(p->GLid);

	for(int i=0; i<p->num_clmem; i++)
		clReleaseMemObject(p->CLmem[i]);
	free(p->CLmem);

	free(p);
}


void ocl_rebuild(OCLPROGRAM *clprog)
{
	if(!clprog)return;
	clprog->happy = 0;
	for(int i=0; i<clprog->num_kernels; i++)
		clReleaseKernel(clprog->k[i]);
	clprog->num_kernels = 0;
	if(clprog->k)
	{
		free(clprog->k);
		clprog->k = NULL;
	}
	clReleaseProgram(clprog->pr);

	int error = 0;
	const char *src = loadTextFile(clprog->filename);
	size_t size = strlen(src);
	cl_int ret;
	cl_program p = clCreateProgramWithSource(OpenCL->c, 1, &src, &size, &ret);
	if(ret != CL_SUCCESS)
	{
		printf("clCreateProgram():%s\n", clStrError(ret));
		error++;
	}
	ret = clBuildProgram(p, 1, OpenCL->d, NULL, NULL, NULL);

	if(ret != CL_SUCCESS)
	{
		int esize = 100000;
		char *errstr = malloc(esize);
		memset(errstr, 0, esize);
		clGetProgramBuildInfo(p, *OpenCL->d, CL_PROGRAM_BUILD_LOG, esize,errstr,NULL);
		printf("clBuildProgram(\"%s\")%s\n%s\n", clprog->filename, clStrError(ret), errstr);
		error++;
	}

	clGetProgramInfo(p, CL_PROGRAM_BINARY_SIZES,
				sizeof(size), &size, NULL);
//	printf("Success: %zu bytes.\n", size);

	cl_uint nk=0;
	clCreateKernelsInProgram(p, 0, NULL, &nk);
	if(!nk)
	{
		printf("no kernels found\n");
		error++;
	}

	cl_kernel *k = malloc(sizeof(cl_kernel)*nk);
	memset(k, 0, sizeof(cl_kernel)*nk);
	ret = clCreateKernelsInProgram(p, nk, k, NULL);

	for(int i=0; i<nk; i++)
	{
		char buf[256];
		memset(buf, 0, 256);
		clGetKernelInfo(k[i], CL_KERNEL_FUNCTION_NAME, 255, buf, NULL);
		cl_uint argc = 0;
		clGetKernelInfo(k[i], CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &argc, NULL);
		printf("Kernel[%d]:%s(%d)\n", i, buf, argc);
	}

	clprog->pr = p;
	clprog->k = k;
	clprog->num_kernels = nk;
	if(!error)clprog->happy = 1;
}

OCLPROGRAM* ocl_build(char *filename)
{
	if(!OpenCL->happy)return 0;

	OCLPROGRAM *clprog = malloc(sizeof(OCLPROGRAM));
	memset(clprog, 0, sizeof(OCLPROGRAM));
	clprog->filename = hcopy(filename);

	if(strstr(filename, "Voxel"))
	{
		clprog->type = 1;
	}
		
	// Allocate the appropriate buffers.
	// Everyone gets an imgout buffer
	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sys_width, sys_height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	cl_int ret;
	cl_mem output = clCreateFromGLTexture2D(OpenCL->c, CL_MEM_WRITE_ONLY,
		GL_TEXTURE_2D, 0, id, &ret);
	if(ret != CL_SUCCESS)
	{
		printf("clCreateFromGLTexture2D():%s\n", clStrError(ret));
	}
	
	switch(clprog->type)
	{
		case 1:
			clprog->num_glid = 2;
			clprog->num_clmem = 8;
			break;
		default:
			clprog->num_glid = 1;
			clprog->num_clmem = 1;
			break;
	}

	clprog->GLid = malloc(sizeof(GLuint)*clprog->num_glid);
	clprog->CLmem = malloc(sizeof(cl_mem)*clprog->num_clmem);
	clprog->GLid[0] = id;
	clprog->CLmem[0] = output;

	switch(clprog->type) {
	case 1:
		// create GL brick texture
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
			GL_TEXTURE_3D, 0, id, &ret);
		if(ret != CL_SUCCESS)
		{
			printf("clCreateFromGLTexture3D():%s\n", clStrError(ret));
		}

		// And the buffers... NodeNode, NodeBrick,
		// NodeUseTime, NodeReqTime, BrickUseTime
		size_t size = 100000*4;
		clprog->CLmem[2] = clCreateBuffer(OpenCL->c,
			CL_MEM_READ_ONLY, sizeof(cl_float)*8, NULL, &ret);
		if(ret != CL_SUCCESS)printf("clvox:buffer 2:%s\n", clStrError(ret));
		clprog->CLmem[3] = clCreateBuffer(OpenCL->c,
			CL_MEM_READ_WRITE, size, 0, &ret);
		if(ret != CL_SUCCESS)printf("clvox:buffer 4:%s\n", clStrError(ret));
		clprog->CLmem[4] = clCreateBuffer(OpenCL->c,CL_MEM_READ_WRITE,size,0,&ret);
		if(ret != CL_SUCCESS)printf("clvox:buffer 5:%s\n", clStrError(ret));
		clprog->CLmem[5] = clCreateBuffer(OpenCL->c,CL_MEM_READ_WRITE,size,0,&ret);
		if(ret != CL_SUCCESS)printf("clvox:buffer 6:%s\n", clStrError(ret));
		clprog->CLmem[6] = clCreateBuffer(OpenCL->c,CL_MEM_READ_WRITE,size,0,&ret);
		if(ret != CL_SUCCESS)printf("clvox:buffer 7:%s\n", clStrError(ret));
		clprog->CLmem[7] = clCreateBuffer(OpenCL->c,CL_MEM_READ_WRITE,size,0,&ret);
		if(ret != CL_SUCCESS)printf("clvox:buffer 8:%s\n", clStrError(ret));
		break;
	default:
		break;
	}

	ocl_add(clprog);
	ocl_rebuild(clprog);
	return clprog;
}

void ocl_loop(void)
{
	if(!OpenCL)return;
	if(!OpenCL->happy)return;

	extern float4 pos, angle;

	for(int i=0; i<OpenCL->num_progs; i++)
	{
		OCLPROGRAM *p = OpenCL->progs[i];
		if(!p)continue;
		if(!p->happy)continue;

		widget *w = p->window;

		cl_int ret;
//	cl_event e;
		size_t work_size[] = { abs(w->size.x - 20), abs(w->size.y - 50) };

		glFinish();
		ret = clEnqueueAcquireGLObjects(OpenCL->q, 1, &p->CLmem[0], 0, 0, 0);
		if(ret != CL_SUCCESS)printf("clEnqueueAcquireGLObjects():%s\n",	clStrError(ret));

		cl_kernel k = OpenCL->progs[i]->k[0];
		ret = clSetKernelArg(k, 0, sizeof(cl_mem), &p->CLmem[0]);
		if(ret != CL_SUCCESS)printf("clSetKernelArg():%s\n", clStrError(ret));

		switch(p->type)
		{
		case 1:
			ret = clEnqueueWriteBuffer(OpenCL->q, p->CLmem[2], CL_TRUE, 0, 16,
				&pos, 0, NULL, NULL);
			if(ret != CL_SUCCESS)printf("clEnqueueWriteBuffer():%s\n",	clStrError(ret));
			ret = clEnqueueWriteBuffer(OpenCL->q, p->CLmem[2], CL_TRUE, 16, 16,
				&angle, 0, NULL, NULL);
			if(ret != CL_SUCCESS)printf("clEnqueueWriteBuffer():%s\n",	clStrError(ret));
//			clEnqueueWriteBuffer(OpenCL->q, p->CLmem[2], CL_TRUE, 0, sizeof(float)*4, &angle, 0, NULL, NULL);
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
			break;
		
		default:
			clSetKernelArg(k, 1, sizeof(float), &time);
			break;
		}

		ret = clEnqueueNDRangeKernel(OpenCL->q, k, 2,NULL, work_size,
				NULL, 0, NULL, NULL);
		if(ret != CL_SUCCESS)
		{
			p->happy = 0;
			printf("clEnqueueNDRangeKernel():%s\n", clStrError(ret));
		}


		ret = clEnqueueReleaseGLObjects(OpenCL->q, 1, &p->CLmem[0], 0, 0, 0);
		if(ret != CL_SUCCESS)printf("clEnqueueReleaseGLObjects():%s\n", clStrError(ret));

		switch(p->type) {
		case 1:
			ret = clEnqueueReleaseGLObjects(OpenCL->q, 1, &p->CLmem[1], 0, 0, 0);
			if(ret != CL_SUCCESS)printf("clEnqueueReleaseGLObjects():%s\n", clStrError(ret));
			break;
		default:
			break;
		}

		clFinish(OpenCL->q);
	//	clWaitForEvents(1, &e);
	}

}


static char* clStrError(cl_int error)
{
	switch(error) {
	case CL_SUCCESS: return "CL_SUCCESS";
	case CL_DEVICE_NOT_FOUND: return "CL_DEVICE_NOT_FOUND";
	case CL_DEVICE_NOT_AVAILABLE: return "CL_DEVICE_NOT_AVAILABLE";
	case CL_COMPILER_NOT_AVAILABLE: return "CL_COMPILER_NOT_AVAILABLE";
	case CL_MEM_OBJECT_ALLOCATION_FAILURE: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
	case CL_OUT_OF_RESOURCES: return "CL_OUT_OF_RESOURCES";
	case CL_OUT_OF_HOST_MEMORY: return "CL_OUT_OF_HOST_MEMORY";
	case CL_PROFILING_INFO_NOT_AVAILABLE: return "CL_PROFILING_INFO_NOT_AVAILABLE";
	case CL_MEM_COPY_OVERLAP: return "CL_MEM_COPY_OVERLAP";
	case CL_IMAGE_FORMAT_MISMATCH: return "CL_IMAGE_FORMAT_MISMATCH";
	case CL_IMAGE_FORMAT_NOT_SUPPORTED: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
	case CL_BUILD_PROGRAM_FAILURE: return "CL_BUILD_PROGRAM_FAILURE";
	case CL_MAP_FAILURE: return "CL_MAP_FAILURE";
	case CL_MISALIGNED_SUB_BUFFER_OFFSET: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
	case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
	case CL_COMPILE_PROGRAM_FAILURE: return "CL_COMPILE_PROGRAM_FAILURE";
	case CL_LINKER_NOT_AVAILABLE: return "CL_LINKER_NOT_AVAILABLE";
	case CL_LINK_PROGRAM_FAILURE: return "CL_LINK_PROGRAM_FAILURE";
	case CL_DEVICE_PARTITION_FAILED: return "CL_DEVICE_PARTITION_FAILED";
	case CL_KERNEL_ARG_INFO_NOT_AVAILABLE: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
	case CL_INVALID_VALUE: return "CL_INVALID_VALUE";
	case CL_INVALID_DEVICE_TYPE: return "CL_INVALID_DEVICE_TYPE";
	case CL_INVALID_PLATFORM: return "CL_INVALID_PLATFORM";
	case CL_INVALID_DEVICE: return "CL_INVALID_DEVICE";
	case CL_INVALID_CONTEXT: return "CL_INVALID_CONTEXT";
	case CL_INVALID_QUEUE_PROPERTIES: return "CL_INVALID_QUEUE_PROPERTIES";
	case CL_INVALID_COMMAND_QUEUE: return "CL_INVALID_COMMAND_QUEUE";
	case CL_INVALID_HOST_PTR: return "CL_INVALID_HOST_PTR";
	case CL_INVALID_MEM_OBJECT: return "CL_INVALID_MEM_OBJECT";
	case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
	case CL_INVALID_IMAGE_SIZE: return "CL_INVALID_IMAGE_SIZE";
	case CL_INVALID_SAMPLER: return "CL_INVALID_SAMPLER";
	case CL_INVALID_BINARY: return "CL_INVALID_BINARY";
	case CL_INVALID_BUILD_OPTIONS: return "CL_INVALID_BUILD_OPTIONS";
	case CL_INVALID_PROGRAM: return "CL_INVALID_PROGRAM";
	case CL_INVALID_PROGRAM_EXECUTABLE: return "CL_INVALID_PROGRAM_EXECUTABLE";
	case CL_INVALID_KERNEL_NAME: return "CL_INVALID_KERNEL_NAME";
	case CL_INVALID_KERNEL_DEFINITION: return "CL_INVALID_KERNEL_DEFINITION";
	case CL_INVALID_KERNEL: return "CL_INVALID_KERNEL";
	case CL_INVALID_ARG_INDEX: return "CL_INVALID_ARG_INDEX";
	case CL_INVALID_ARG_VALUE: return "CL_INVALID_ARG_VALUE";
	case CL_INVALID_ARG_SIZE: return "CL_INVALID_ARG_SIZE";
	case CL_INVALID_KERNEL_ARGS: return "CL_INVALID_KERNEL_ARGS";
	case CL_INVALID_WORK_DIMENSION: return "CL_INVALID_WORK_DIMENSION";
	case CL_INVALID_WORK_GROUP_SIZE: return "CL_INVALID_WORK_GROUP_SIZE";
	case CL_INVALID_WORK_ITEM_SIZE: return "CL_INVALID_WORK_ITEM_SIZE";
	case CL_INVALID_GLOBAL_OFFSET: return "CL_INVALID_GLOBAL_OFFSET";
	case CL_INVALID_EVENT_WAIT_LIST: return "CL_INVALID_EVENT_WAIT_LIST";
	case CL_INVALID_EVENT: return "CL_INVALID_EVENT";
	case CL_INVALID_OPERATION: return "CL_INVALID_OPERATION";
	case CL_INVALID_GL_OBJECT: return "CL_INVALID_GL_OBJECT";
	case CL_INVALID_BUFFER_SIZE: return "CL_INVALID_BUFFER_SIZE";
	case CL_INVALID_MIP_LEVEL: return "CL_INVALID_MIP_LEVEL";
	case CL_INVALID_GLOBAL_WORK_SIZE: return "CL_INVALID_GLOBAL_WORK_SIZE";
	case CL_INVALID_PROPERTY: return "CL_INVALID_PROPERTY";
	case CL_INVALID_IMAGE_DESCRIPTOR: return "CL_INVALID_IMAGE_DESCRIPTOR";
	case CL_INVALID_COMPILER_OPTIONS: return "CL_INVALID_COMPILER_OPTIONS";
	case CL_INVALID_LINKER_OPTIONS: return "CL_INVALID_LINKER_OPTIONS";
	case CL_INVALID_DEVICE_PARTITION_COUNT: return "CL_INVALID_DEVICE_PARTITION_COUNT";
	default:return "Unknown";
	}
}
	


