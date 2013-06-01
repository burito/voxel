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


#include <GL/glew.h>

#include "mesh.h"


static MESH* read_obj(char * filename)
{
	FILE *fptr = fopen(filename, "r");
	if(!fptr)return 0;

	char* buf = malloc(1024);

	char c;
	float x,y,z;
	int f1, f2, f3;
	char *ret;
	int nVert=0;
	int nFace=0;

	// count the verts & faces for alloc
	do {
		ret = fgets(buf, 1024, fptr);
		if(!ret)break;
		switch(buf[0]) {
		case 'v':
			nVert++;
			break;
		case 'f':
			nFace++;
			break;
		}
	} while(ret);
//	rewind(fptr);
	fclose(fptr);
	fptr = fopen(filename, "r");


	float3 *verts = malloc(sizeof(float3)*nVert);
	int3 *faces = malloc(sizeof(int3)*nFace);

	nVert = 0;
	nFace = 0;
	do {
		ret = fgets(buf, 1024, fptr);
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
	} while(ret);
	free(buf);
	fclose(fptr);

	MESH *m = malloc(sizeof(MESH));

	m->v = verts;
	m->nv = nVert;
	m->f = faces;
	m->nf = nFace;
	m->n = NULL;
	m->nn = 0;
	m->vbo = m->ebo = 0;

	return m;
}


static void gen_normals(MESH* m)
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


static void mesh_bound(float3 *m, int count)
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

typedef struct LLIST
{
	int face;
	struct LLIST* next;
} LLIST;

static void gen_vertex_normals(MESH *m)
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

void mesh_write(MESH* m, char *filename)
{
	FILE *fptr = fopen(filename, "wb");
	int zero = 0;

	fwrite("FMSH", 4, 1, fptr);
	fwrite(&m->nv, 4, 1, fptr);
	fwrite(&m->nf, 4, 1, fptr);
	fwrite(&zero, 4, 1, fptr);

	for(int i=0; i<m->nv; i++)
	{
		fwrite(&m->p[i*2].x, 4, 1, fptr);
		fwrite(&m->p[i*2].y, 4, 1, fptr);
		fwrite(&m->p[i*2].z, 4, 1, fptr);
//		fwrite(&zero, 4, 1, fptr);
		fwrite(&m->p[i*2+1].x, 4, 1, fptr);
		fwrite(&m->p[i*2+1].y, 4, 1, fptr);
		fwrite(&m->p[i*2+1].z, 4, 1, fptr);
//		fwrite(&zero, 4, 1, fptr);
	}

	for(int i=0; i<m->nf; i++)
	{
		fwrite(&m->f[i].x, 4, 1, fptr);
		fwrite(&m->f[i].y, 4, 1, fptr);
		fwrite(&m->f[i].z, 4, 1, fptr);
	}

	fclose(fptr);
}


static void mesh_interleave(MESH *m)
{
	float3 *p = malloc(m->nv * 24);
	for(int i=0; i < m->nv; i++)
	{
		p[i*2  ] = m->v[i];
		p[i*2+1] = m->n[i];
	}
	m->p = p;
	free(m->v); m->v = 0;
	free(m->n); m->n = 0;
}


static void mesh_vbo_load(MESH *m)
{
	glGenBuffers(1, &m->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
	glBufferData(GL_ARRAY_BUFFER, m->nv*24, m->p, GL_STATIC_DRAW);

	glGenBuffers(1, &m->ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->nf*12, m->f, GL_STATIC_DRAW);
	int *p = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	memcpy(p, m->f, m->nf);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


void mesh_free(MESH *m)
{
	if(!m)return;
	if(m->ebo)glDeleteBuffers(1, &m->ebo);
	if(m->vbo)glDeleteBuffers(1, &m->vbo);
	if(m->v)free(m->v);
	if(m->n)free(m->n);
	if(m->p)free(m->p);
	if(m->f)free(m->f);
	free(m);
}


void mesh_draw(MESH *m)
{
	glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
	glVertexPointer(3, GL_FLOAT, 24, (GLvoid*)0);
	glNormalPointer(GL_FLOAT, 24, (GLvoid*)12);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);

	glDrawElements(GL_TRIANGLES, m->nf*12, GL_UNSIGNED_INT, (GLvoid*)0);
//	glDrawArrays(GL_POINTS, 0, m->nv);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

MESH* mesh_load_obj(char *filename)
{
	MESH *m = read_obj(filename);
	gen_normals(m);
	gen_vertex_normals(m);
	mesh_bound(m->v, m->nv);
	mesh_interleave(m);
	mesh_vbo_load(m);
	return m;
}


MESH* mesh_load(char *filename)
{
	MESH *m = malloc(sizeof(MESH));
	FILE *fptr = fopen(filename, "rb");
	char magic[] = "ERRR";

	fread(magic, 4, 1, fptr);
	// check the magic bytes?
	fread(&m->nv, 4, 1, fptr);
	fread(&m->nf, 4, 1, fptr);
	fread(magic, 4, 1, fptr);
	
	m->p = malloc(m->nv*24);
	fread(m->p, m->nv*24, 1, fptr);
	m->f = malloc(m->nf*12);
	fread(m->f, m->nf*12, 1, fptr);
	fclose(fptr);

	mesh_vbo_load(m);
	return m;
}



#ifdef STATIC_TEST

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
			printf("Reading Object.\n");
			m = read_obj(argv[1]);
			printf("Normals\n");
			gen_normals(m);
			printf("Vertex normals\n");
			gen_vertex_normals(m);
			printf("bounding the mesh\n");
			mesh_bound(m->v, m->nv);
			printf("writing\n");
			mesh_file_write(argv[2], m);
			break;
		default:
			printf("user error.\n");
			return 1;
	}

	return 0;
}

#endif /* STATIC_TEST */

