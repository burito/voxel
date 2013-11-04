
#include <CL/cl.h>

typedef struct OCLCONTEXT
{
	cl_uint num_pid, *num_did;
	cl_platform_id *pid, *p;
	cl_device_id **did, *d;
	cl_context c;
	cl_command_queue q;
	cl_program pr;
	cl_kernel *k;
	int num_kernels;
	GLuint id;
	cl_mem i;
} OCLCONTEXT;



OCLCONTEXT* ocl_init(void);
void ocl_free(OCLCONTEXT *c);
void ocl_loop(OCLCONTEXT *c);
void ocl_build(OCLCONTEXT *c, char *filename);

