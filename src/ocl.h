
#include <CL/cl.h>

typedef struct OCLPROGRAM
{
	int happy;
	char *filename;
	cl_program pr;
	cl_kernel *k;
	int num_kernels;
	GLuint *GLid;
	int num_glid;
	cl_mem *CLmem;
	int num_clmem;
	int type;
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

