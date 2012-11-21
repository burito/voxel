/*
Copyright (c) 2012 Daniel Burke

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "3dmaths.h"


typedef struct MESH
{
	int nv, nn, nf;
	float3 *v, *n;
	int3 *f;
} MESH;

MESH* read_obj(char * filename)
{
	FILE *fptr = fopen(filename, "r");
	if(!fptr)return 0;

	char* buf = malloc(1024);

	char c;
	float x,y,z;
	int f1, f2, f3;
	int ret = 1;
	int nVert=0;
	int nFace=0;

	// count the verts & faces for alloc
	while(ret) {
		ret = (int)fgets(buf, 1024, fptr);
		if(!ret)break;
		switch(buf[0]) {
		case 'v':
			nVert++;
			break;
		case 'f':
			nFace++;
			break;
		}
	}
//	rewind(fptr);
	fclose(fptr);
	fptr = fopen(filename, "r");


	float3 *verts = malloc(sizeof(float3)*nVert);
	int3 *faces = malloc(sizeof(int3)*nFace);

	nVert = 0;
	nFace = 0;
	ret = 1;
	while(ret) {
		ret = (int)fgets(buf, 1024, fptr);
		if(!ret)break;
		switch(buf[0]) {
		case 'v':
			sscanf(buf, "%c %f %f %f\n", &c, &x, &y, &z );
			verts[nVert].x = x;
			verts[nVert].y = y;
			verts[nVert].z = z;
			nVert++;
			break;
		case 'f':
			sscanf(buf, "%c %d %d %d\n", &c, &f1, &f2, &f3 );
			faces[nFace].x = f1-1;
			faces[nFace].y = f2-1;
			faces[nFace].z = f3-1;
			nFace++;
			break;
		default:
			break;
		}
	}
	free(buf);
	fclose(fptr);

	MESH *m = malloc(sizeof(MESH));

	m->v = verts;
	m->nv = nVert;
	m->f = faces;
	m->nf = nFace;
	m->n = NULL;
	m->nn = 0;

	return m;
}


void gen_normals(MESH* m)
{
	int i;	
	float3 a, b, t;
	float3 *n = malloc(sizeof(float3)*m->nf);

	// Generate per face normals
	for(i=0; i<m->nf; i++)
	{
		vect_sub( &a, &m->v[m->f[i].x], &m->v[m->f[i].y] );
		vect_sub( &b, &m->v[m->f[i].x], &m->v[m->f[i].z] );
		vect_cross( &t, &b, &a );
		vect_norm( &n[i], &t);
	}

	m->n = n;
	m->nn = m->nf;
}

typedef struct LLIST
{
	int face;
	struct LLIST* next;
} LLIST;

void mesh_bound(float3 *m, int count)
{
	float3 min, max, size;

	F3COPY(min, m[0]);
	F3COPY(max, m[0]);

	for(int i=0; i<count; i++)
	{
		if(m[i].x < min.x)min.x = m[i].x;
		if(m[i].x > max.x)max.x = m[i].x;

		if(m[i].y < min.y)min.y = m[i].y;
		if(m[i].y > max.y)max.y = m[i].y;

		if(m[i].z < min.z)min.z = m[i].z;
		if(m[i].z > max.z)max.z = m[i].z;
	}

	F3SUB(size, max, min);
	float longest = F3MAX(size);
	
	for(int i=0; i<count; i++)
	{
		F3SUB(m[i], m[i], min);
		vect_sdivide(&m[i], &m[i], longest);
	}

	vect_sdivide(&size, &size, longest);
	printf("Volume = (%f, %f, %f)\n", size.x, size.y, size.z);
}


void gen_vertex_normals(MESH *m)
{
	int i;

	LLIST *vert = malloc(sizeof(LLIST)*m->nv);
	memset(vert, 0, sizeof(LLIST)*m->nv);

	LLIST *tmp;
	int index;

	// tell each vert about it's faces
	for(i=0; i<m->nf; i++)
	{
		index = m->f[i].x;
		tmp = malloc(sizeof(LLIST));
		tmp->face = i;
		tmp->next = vert[index].next;
		vert[index].next = tmp;

		index = m->f[i].y;
		tmp = malloc(sizeof(LLIST));
		tmp->face = i;
		tmp->next = vert[index].next;
		vert[index].next = tmp;

		index = m->f[i].z;
		tmp = malloc(sizeof(LLIST));
		tmp->face = i;
		tmp->next = vert[index].next;
		vert[index].next = tmp;
	}

	float3 *no = malloc(sizeof(float3)*m->nv);	// normal out
	float3 t;
	// now average the normals and store in the output
	for(i=0; i<m->nv; i++)
	{
		tmp = vert[i].next;
		t.x = t.y = t.z = 0;
		while(tmp)
		{
			vect_add( &t, &t, &m->n[tmp->face]);
			LLIST *last = tmp;
			tmp = tmp->next;
			free(last);
		}
		vect_norm(&no[i], &t);
	}
	free(vert);

	free(m->n);
	m->n = no;
	m->nn = m->nv;
}




void write_file(char* filename, MESH* m)
{
	int i;
	FILE *fptr = fopen(filename, "wb");

	int zero = 0;

	int vbo = m->nv;
	int ebo = m->nf;

	fwrite("FMSH", 4, 1, fptr);
	fwrite(&vbo, 4, 1, fptr);
	fwrite(&ebo, 4, 1, fptr);
	fwrite(&zero, 4, 1, fptr);

	for(i=0; i<m->nv; i++)
	{
		fwrite(&m->v[i].x, 4, 1, fptr);
		fwrite(&m->v[i].y, 4, 1, fptr);
		fwrite(&m->v[i].z, 4, 1, fptr);
//		fwrite(&zero, 4, 1, fptr);
		fwrite(&m->n[i].x, 4, 1, fptr);
		fwrite(&m->n[i].y, 4, 1, fptr);
		fwrite(&m->n[i].z, 4, 1, fptr);
//		fwrite(&zero, 4, 1, fptr);
	}

	for(i=0; i<m->nf; i++)
	{
		fwrite(&m->f[i].x, 4, 1, fptr);
		fwrite(&m->f[i].y, 4, 1, fptr);
		fwrite(&m->f[i].z, 4, 1, fptr);
	}

	fclose(fptr);
}


int main(int argc, char *argv[])
{
	MESH *m;
	switch(argc) {
		case 2:
			m = read_obj(argv[1]);
			gen_normals(m);
			printf("Verts=%d, Face=%d, Normal=%d\n", m->nv, m->nf, m->nn);
			break;
		case 3:
			m = read_obj(argv[1]);
			gen_normals(m);
			gen_vertex_normals(m);
			mesh_bound(m->v, m->nv);
			write_file(argv[2], m);
			break;
		default:
			printf("user error.\n");
			return 1;
	}

	return 0;
}

