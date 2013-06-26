
#include "3dmaths.h"
#include "image.h"


typedef struct WF_MTL
{
	float Ns, Ni;		// specular coefficient, ?
	float3 Ka, Kd, Ks, Ke;	// ambient, diffuse, specular, emmit
	float4 colour;		// colour + alpha
	IMG *map_Ka, *map_Kd, *map_d, *map_bump;	// amb, spec, alpha, bump
	char *name;
	struct WF_MTL *next;
	int nf;
} WF_MTL;

typedef struct WF_FACE
{
	int3 f, t, n;
	float3 normal;
	int s, g;
	WF_MTL *m;
} WF_FACE;


typedef struct WF_OBJ
{
	WF_MTL *m;
	WF_FACE *f;
	int3 *vf;
	int nv, nn, nt, nf, ng, ns, nm;
	float3 *v, *vt, *vn, *fn;
	float2 *uv;
	GLfloat *p;
	char *filename;
	char **groups;
	int *sgroups;
	GLuint vbo, ebo;
} WF_OBJ;


void mtl_begin(WF_MTL *m);
void mtl_end(void);

WF_OBJ* wf_load(char *filename);
void wf_draw(WF_OBJ *w);
void wf_free(WF_OBJ *w);


