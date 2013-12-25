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

#include <CL/cl.h>

typedef struct OCLPROGRAM
{
	int happy;
	char *filename;
	cl_program pr;
	cl_kernel *k;
	int num_kernels;
	int num_mem;
	GLuint *GLid;
	GLuint *GLtype;
	cl_mem *CLmem;
	int type;
	void* window;
} OCLPROGRAM;

typedef struct OCLCONTEXT
{
	int happy;
	cl_uint num_pid, *num_did;
	cl_platform_id *pid, *p;
	cl_device_id **did, *d;
	cl_context c;
	cl_command_queue q;

	OCLPROGRAM **progs;
	int num_progs;
} OCLCONTEXT;

extern OCLCONTEXT *OpenCL;


int ocl_init(void);
void ocl_end(void);
void ocl_free(OCLPROGRAM *p);
void ocl_loop();
OCLPROGRAM* ocl_build(char *filename);
void ocl_rebuild(OCLPROGRAM *clprog);
void ocl_rm(OCLPROGRAM *clprog);
char* clStrError(cl_int error);

void ocl_gltex2d(OCLPROGRAM *p, int2 size, GLuint type, GLuint format);
void ocl_gltex3d(OCLPROGRAM *p, int3 size, GLuint type, GLuint format);
void ocl_glbuf(OCLPROGRAM *p, int size, void *ptr);

void ocl_acquire(OCLPROGRAM *p);
void ocl_release(OCLPROGRAM *p);


