
#include "3dmaths.h"

typedef struct MESH
{
	int nv, nn, nf;
	float3 *v, *n, *p;
	int3 *f;
	GLuint vbo, ebo;
} MESH;

MESH* mesh_load_obj(char *filename);
MESH* mesh_load(char *filename);
void mesh_write(MESH *m, char *filename);
void mesh_draw(MESH *m);
void mesh_free(MESH *m);

