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

#define NODE_COUNT 100000
#define BRICK_SIZE 8
#define BRICK_EDGE 64
#define BRICK_COUNT (BRICK_EDGE*BRICK_EDGE*BRICK_EDGE)

GLSLVOXEL *glVox=NULL;
OCLPROGRAM *clVox=NULL;
int voxel_rebuildkernel_flag=0;
int voxel_rebuildshader_flag=0;
int Knl_Render;
int Knl_ResetNodeTime;
int Knl_ResetBrickTime;

int use_glsl = 1;

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
	}
}

static void voxel_ResetTime(void)
{
	// Tell the GPU to reset the time buffers
	glFinish();
	ocl_acquire(clVox);
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
	ocl_release(clVox);
}

int loc_cam, loc_time;


void voxel_glslrebuild(void)
{
	if(glVox->r_vert)glDeleteShader(glVox->r_vert);
	glVox->r_vert = 0;
	if(glVox->r_frag)glDeleteShader(glVox->r_frag);
	glVox->r_frag = 0;
	if(glVox->render)glDeleteProgram(glVox->render);
	glVox->render = 0;

	GLint vert = shader_load(GL_VERTEX_SHADER, "data/Vertex.GLSL");
	GLint frag = shader_load(GL_FRAGMENT_SHADER, "data/Voxel.GLSL");
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);
	GLint param;
	glGetProgramiv(prog, GL_LINK_STATUS, &param);
	printf("*** Shader linking %s.\n",
		param == GL_FALSE ? "went as expected" : "worked");
	printProgramInfoLog(prog);

	if(param == GL_FALSE)
		glVox->happy = 0;
	else
		glVox->happy = 1;
	
//	loc_cam = glGetUniformLocation(prog, "camera");
	loc_time = glGetUniformLocation(prog, "time");


	GLuint ri_camera;

	GLuint ssb = GL_SHADER_STORAGE_BLOCK;

	ri_camera = glGetProgramResourceIndex(glVox->render, ssb, "camera");


//	glShaderStorageBlockBinding(glVox->render, ri_camera, 0);




	glVox->r_vert = vert;
	glVox->r_frag = frag;
	glVox->render = prog;

}


void voxel_init(void)
{
	clVox = ocl_build("./data/Voxel.OpenCL");
	ocl_rm(clVox);
	if(clVox->happy)voxel_FindKernels();
	
	size_t size = NODE_COUNT*8*4;
	int3 cube = {512, 512, 512};
	ocl_gltex3d(clVox, cube, GL_RGBA, GL_UNSIGNED_BYTE);
	ocl_glbuf(clVox, sizeof(cl_float)*8, NULL);	// camera
	ocl_glbuf(clVox, size, NULL);	// NodeNode
	ocl_glbuf(clVox, size, NULL);	// NodeBrick
	ocl_glbuf(clVox, size, NULL);	// NodeUseTime
	ocl_glbuf(clVox, size, NULL);	// NodeReqTime
	ocl_glbuf(clVox, size, NULL);	// BrickReqTime
	ocl_glbuf(clVox, BRICK_COUNT*4, NULL);	// BrickUseTime

	voxel_ResetTime();

	if(clVox->happy)printf("everything is happy\n");

	glVox = malloc(sizeof(GLSLVOXEL));
	memset(glVox, 0, sizeof(GLSLVOXEL));
	
	voxel_glslrebuild();
}




extern float4 pos;
extern float4 angle;


void voxel_loop(void)
{
	if(!clVox)return;

	if(voxel_rebuildkernel_flag)
	{
		voxel_rebuildkernel_flag = 0;
		ocl_rebuild(clVox);
		if(clVox->happy)voxel_FindKernels();
	}

	if(voxel_rebuildshader_flag)
	{
		voxel_rebuildshader_flag = 0;
		voxel_glslrebuild();
	}


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

		if(!glVox)return;
		if(!glVox->happy)return;
		glUseProgram(glVox->render);


		glBindBuffer(GL_SHADER_STORAGE_BUFFER, clVox->GLid[2]);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 16, &pos);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 16, 16, &angle);

//		GLint x;
//		glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &x);
//		printf("max ssbb=%d\n", x);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, clVox->GLid[2]); // cam
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, clVox->GLid[3]); // nn
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, clVox->GLid[4]); // nb
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, clVox->GLid[5]); // nut
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, clVox->GLid[6]); // nrt
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, clVox->GLid[7]); // brt
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, clVox->GLid[8]); // but

//		GLuint bi = glGetUniformBlockIndex(glVox->render, "camera");
//		glUniformBlockBinding(glVox->render, bi, clVox->GLid[2]);
//		glUniformBlockBinding(glVox->render, bi, GL_SHADER_STORAGE_BUFFER);
//		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		glUniform1f(loc_time, time);


		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D, clVox->GLid[1]);
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
	glOrtho(0, vid_width, 0, vid_height, -1, 5000);
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
}

void voxel_end(void)
{

	ocl_free(clVox);

}


void voxel_open(char *filename)
{



}


