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

#include "3dmaths.h"
#include "mesh.h"
#include "text.h"

#define BUF_LEN 1024

#ifndef STATIC_TEST
void mtl_begin(WF_MTL *m)
{
	GLfloat spec[] = {m->Ks.x, m->Ks.y, m->Ks.z, 1.0f};
	GLfloat amb[] = {m->Ka.x, m->Ka.y, m->Ka.z, 1.0f};
	GLfloat diff[] = {m->Kd.x, m->Kd.y, m->Kd.z, 1.0f};
	GLfloat emit[] = {m->Ke.x, m->Ke.y, m->Ke.z, 1.0f};
	glMateriali(GL_FRONT, GL_SHININESS, m->Ns);
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
	glMaterialfv(GL_FRONT, GL_EMISSION, emit);
	glColor4f(m->colour.x, m->colour.y, m->colour.z, m->colour.w);
	if(m->map_Ka)
	{
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, m->map_Ka->id);
	}
	if(m->map_Kd)
	{
		glActiveTexture(GL_TEXTURE1);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, m->map_Kd->id);
	}
	if(m->map_d)
	{
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, m->map_d->id);
	}
	if(m->map_bump)
	{
		glActiveTexture(GL_TEXTURE3);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, m->map_bump->id);

	}
	glActiveTexture(GL_TEXTURE0);
}
void mtl_end(void)
{
	GLfloat amb[] = {0.2f, 0.2f, 0.2f, 1.0f};
	GLfloat diff[] = {0.8f, 0.8f, 0.8f, 1.0f};
	GLfloat zero[] = {0.0f, 0.0f, 0.0f, 1.0f};
	glMateriali(GL_FRONT, GL_SHININESS, 0);
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
	glMaterialfv(GL_FRONT, GL_SPECULAR, zero);
	glMaterialfv(GL_FRONT, GL_EMISSION, zero);

	glColor4f(1,1,1,1);
	glMateriali(GL_FRONT, GL_SHININESS, 0);
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE2);
	glDisable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE3);
	glDisable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
}
#endif
void mtl_free(WF_MTL *m)
{
	if(!m)return;
	if(m->name)free(m->name);
	if(m->map_Ka)img_free(m->map_Ka);
	if(m->map_Kd)img_free(m->map_Kd);
	if(m->map_d)img_free(m->map_d);
	if(m->map_bump)img_free(m->map_bump);
	free(m);

}

static WF_MTL* mtl_newmtl(char *hostpath, FILE *fptr, char *name)
{
	WF_MTL *m = malloc(sizeof(WF_MTL));
	if(!m)return 0;
	memset(m, 0, sizeof(WF_MTL));
	m->colour.x = m->colour.y = m->colour.z = m->colour.w = 1.0f;
	m->Ns = 50.0f;							// specular coefficient
	m->Ka.x = m->Ka.y = m->Ka.z = 0.2f;		// ambient
	m->Kd.x = m->Kd.y = m->Kd.z = 0.8f;		// diffuse
	m->Ks.x = m->Ks.y = m->Ks.z = 0.2f;		// specular
	m->name = hcopy(name);
	char buf[BUF_LEN];
	float *targetf;
	float3 *targetf3;
	IMG **targeti;
	char *path;

	int i=0;

	long last = ftell(fptr);

	while(fgets(buf, BUF_LEN, fptr))
	{
		tailchomp(buf);
		if(strlen(buf) < 3)break;
		for(i = 0; whitespace(buf[i]); i++);

		switch(buf[i]) {
		case 'N':
			switch(buf[i+1]) {
			default:
			case 's': targetf = &m->Ns; break;		// specular coefficient
			case 'i': targetf = &m->Ni; break;		// ?
			}
			*targetf = fast_atof(buf+i+3);
			break;
		case 'K':
			targetf3 = 0;
			switch(buf[i+1]) {
			case 'a': targetf3 = &m->Ka; break;		// ambient
			case 'd': targetf3 = &m->Kd; break;		// diffuse
			case 's': targetf3 = &m->Ks; break;		// specular
			case 'e': targetf3 = &m->Ke; break;		// emissive
			}
			if(!targetf3)break;
			i+=2;
			sscanf(buf+i, "%f %f %f", &targetf3->x, &targetf3->y, &targetf3->z);
			break;
		case 'm':
			switch(buf[i+5]) {
			case 'a': targeti = &m->map_Ka; break;		// ambient
			case 'd': targeti = &m->map_Kd; break;		// diffuse
			case 'u': targeti = &m->map_bump; break;	// bump
			default:
			case ' ': targeti = &m->map_d;  break;		// alpha
			}
			while(!whitespace(buf[i]))i++;i++;
			path = repath(hostpath, buf+i);
			*targeti = img_load(path);
			free(path);
			break;
		case 'd':
			m->colour.w = fast_atof(buf+i+2);		// alpha
			break;
		case 'T':									// colour?
		sscanf(buf+i, "Tf %f %f %f", &m->colour.x, &m->colour.y, &m->colour.z);
			break;
		case 'n':
			fseek(fptr, last, SEEK_SET);
			return m;
		}
		last = ftell(fptr);
	}
	return m;
}


static void mtl_load(WF_OBJ *w, char *filename)
{
	if(!w)return;
	if(!filename)return;
	if(!w->filename)return;
	char *filepath = repath(w->filename, filename);
	printf("Loading Wavefront MTL(\"%s\");\n", filepath);
	FILE *fptr = fopen(filepath, "r");
	if(!fptr)
	{
		free(filepath);
		return;
	}

	char buf[BUF_LEN];
	while(fgets(buf, BUF_LEN, fptr))
	if(strstr(buf, "newmtl"))
	{
		tailchomp(buf);
		WF_MTL *m = mtl_newmtl(filepath, fptr, buf+7);
		if(m)
		{
			w->nm++;
			m->next = w->m;
			w->m = m;
		}
	}
	free(filepath);
	fclose(fptr);
}



static WF_MTL* find_material(WF_MTL *m, char *name)
{
	if(!m)return 0;
	if(!name)return 0;
	if(!m->name)return 0;
	while(strcmp(m->name, name))
	{
		if(!m->next)return 0;
		m = m->next;
		if(!m->name)return 0;
	}
	return m;
}


static void wf_bound(WF_OBJ *w)
{
	if(!w)return;
	float3 *v = w->v;
	if(!v)return;
	float3 min, max, size, mid;
	max = min = v[0];
	for(int i=0; i<w->nv; i++)
	{
		if(v[i].x < min.x)min.x = v[i].x;
		if(v[i].x > max.x)max.x = v[i].x;

		if(v[i].y < min.y)min.y = v[i].y;
		if(v[i].y > max.y)max.y = v[i].y;

		if(v[i].z < min.z)min.z = v[i].z;
		if(v[i].z > max.z)max.z = v[i].z;
	}

	F3SUB(size, max, min);
	float longest = F3MAX(size);
	F3SSUB(mid, longest, size);
	F3MULS(mid, mid, 0.5);
//	longest = longest * (12.0 / 14.0);
	for(int i=0; i<w->nv; i++)
	{
		F3SUB(v[i], v[i], min);
		F3ADD(v[i], v[i], mid);
		vect_sdivide(&v[i], &v[i], longest);
		F3MULS(v[i], v[i], (12.0 / 14.0));
		F3ADDS(v[i], v[i], (1.0 / 14.0));
	}

	printf("Volume = (%f, %f, %f)\n", size.x, size.y, size.z);
}

void wf_buffer_drawarray(WF_OBJ *w)
{
	int size = w->nf * 32 * 3;
	WF_ARRAY *p = malloc(size);
	memset(p, 0, size);

	printf("At init we have %d/%d/%d\n", w->nv, w->nn, w->nt);

	WF_MTL *m = w->m;
	int o=0;
	for(;m;m = m->next)
	{
		m->nf = 0;
		for(int i=0; i<w->nf; i++)
		if(w->f[i].m == m)
		{
			p[o*3].v = w->v[w->f[i].f.x];
			p[o*3].n = w->vn[w->f[i].f.x];
			p[o*3+1].v = w->v[w->f[i].f.y];
			p[o*3+1].n = w->vn[w->f[i].f.y];
			p[o*3+2].v = w->v[w->f[i].f.z];
			p[o*3+2].n = w->vn[w->f[i].f.z];
			if(w->nt)
			{
				p[o*3].t.x = w->vt[w->f[i].t.x].x;
				p[o*3].t.y = w->vt[w->f[i].t.x].y;
				p[o*3+1].t.x = w->vt[w->f[i].t.y].x;
				p[o*3+1].t.y = w->vt[w->f[i].t.y].y;
				p[o*3+2].t.x = w->vt[w->f[i].t.z].x;
				p[o*3+2].t.y = w->vt[w->f[i].t.z].y;
			}
			o++;
			m->nf++;
		}
	}
	printf("At nomtl we have %d/%d-%d\n", w->nv, o, w->nf);
	
	if(o < w->nf)
	for(int i=0; i<w->nf; i++)
	if(w->f[i].m == 0)
	{
		p[o*3].v = w->v[w->f[i].f.x];
		p[o*3].n = w->vn[w->f[i].f.x];
		p[o*3+1].v = w->v[w->f[i].f.y];
		p[o*3+1].n = w->vn[w->f[i].f.y];
		p[o*3+2].v = w->v[w->f[i].f.z];
		p[o*3+2].n = w->vn[w->f[i].f.z];
		if(w->nt)
		{
			p[o*3].t.x = w->vt[w->f[i].t.x].x;
			p[o*3].t.y = w->vt[w->f[i].t.x].y;
			p[o*3+1].t.x = w->vt[w->f[i].t.y].x;
			p[o*3+1].t.y = w->vt[w->f[i].t.y].y;
			p[o*3+2].t.x = w->vt[w->f[i].t.z].x;
			p[o*3+2].t.y = w->vt[w->f[i].t.z].y;
		}
		o++;
	}
	w->p = (float*)p;
	printf("Interleaved faces ok!\n");
}


void wf_interleave(WF_OBJ *w)
{
	if(!w)return;
	printf("At interleave we have %d/%d/%d\n", w->nv, w->nn, w->nt);
	if(!w->nv)return;
	float *p = malloc(w->nv * 32);
	if(!p)return;
	memset(p, 0, w->nv * 32);
	// interleave the verts, normals and texture coords for the VBO
	for(int i=0; i<w->nv; i++)
	{
		p[i*8+0] = w->v[i].x;
		p[i*8+1] = w->v[i].y;
		p[i*8+2] = w->v[i].z;
		if(w->nv == w->nn)
		{
			p[i*8+3] = w->vn[i].x;
			p[i*8+4] = w->vn[i].y;
			p[i*8+5] = w->vn[i].z;
		}
		if(w->uv)
		{
			p[i*8+6] = w->uv[i].x;
			p[i*8+7] = w->uv[i].y;
		}
		else
		{
			p[i*8+6] = 0.0;
			p[i*8+7] = 0.0;

		}
	}
	printf("Interleaved ok!\n");
	w->p = p;

	// now store the faces, per material for fast rendering
	int3 *f = malloc(w->nf*12);
	WF_MTL *m = w->m;
	int o=0;
	for(;m;m = m->next)
	{
		m->nf = 0;
		for(int i=0; i<w->nf; i++)
		if(w->f[i].m == m)
		{
			f[o++] = w->f[i].f;
			m->nf++;
		}
	}
	
	for(;o < w->nf; o++)
	for(int i=0; i<w->nf; i++)
	if(w->f[i].m == 0)
		f[o++] = w->f[i].f;
	w->vf = f;
	printf("Interleaved faces ok!\n");
//	free(w->v); w->v = 0;
//	free(w->vn); w->vn = 0;
}

static void wf_face_normals(WF_OBJ *w)
{
	if(!w)return;
	if(!w->nf)return;

	float3 a, b, t;

	// Generate per face normals
	for(int i=0; i<w->nf; i++)
	{
		vect_sub( &a, &w->v[w->f[i].f.x], &w->v[w->f[i].f.y] );
		vect_sub( &b, &w->v[w->f[i].f.x], &w->v[w->f[i].f.z] );
		vect_cross( &t, &b, &a );
		vect_norm( &w->f[i].normal, &t);
	}
}


static void wf_vertex_normals(WF_OBJ *w)
{
	if(!w)return;
	if(!w->nf)return;
	if(!w->vn)return;

	typedef struct LLIST
	{
		int face;
		struct LLIST* next;
	} LLIST;

	LLIST *vert = malloc(sizeof(LLIST)*w->nv);
	memset(vert, 0, sizeof(LLIST)*w->nv);

	LLIST *tmp;
	int index;

	// tell each vert about it's faces
	for(int i=0; i<w->nf; i++)
//	if(w->f[i].s == smoothgroup)
	{
		index = w->f[i].f.x;
		tmp = malloc(sizeof(LLIST));
		tmp->face = i;
		tmp->next = vert[index].next;
		vert[index].next = tmp;

		index = w->f[i].f.y;
		tmp = malloc(sizeof(LLIST));
		tmp->face = i;
		tmp->next = vert[index].next;
		vert[index].next = tmp;

		index = w->f[i].f.z;
		tmp = malloc(sizeof(LLIST));
		tmp->face = i;
		tmp->next = vert[index].next;
		vert[index].next = tmp;
	}

	// now average the normals and store in the output
	for(int i=0; i<w->nv; i++)
	{
		tmp = vert[i].next;
		if(!tmp)continue;
		float3 t = {0,0,0};
		while(tmp)
		{
			vect_add( &t, &t, &w->f[tmp->face].normal);
			LLIST *last = tmp;
			tmp = tmp->next;
			free(last);
		}

		if(vect_magnitude(&t)>0.1)
		{
//			if(vect_magnitude(&w->vn[i])>0.1)
//				printf("Vertex copy required\n");
			vect_norm(&w->vn[i], &t);
		}

	}
	free(vert);
}



static void wf_normals(WF_OBJ *w)
{
	if(!w)return;
	if(w->nv == w->nn)return;

	if(w->vn)free(w->vn);
	w->vn = malloc(sizeof(float3)*w->nv);
	memset(w->vn, 0, sizeof(float3)*w->nv);
	w->nn = w->nv;
	
	wf_face_normals(w);
	wf_vertex_normals(w);


}

static void wf_texcoords(WF_OBJ *w)
{
	if(!w)return;
	if(!w->nt)return;
	const int size = w->nv * sizeof(float2);
	w->uv = malloc(size);
	memset(w->uv, 0, size);

	int uvcopy = 0;

	for(int i=0; i<w->nf; i++)
	{
		if(F2MAG(w->uv[w->f[i].f.x]) > 0.1)
		{
			uvcopy++;
		}
		F2COPY(w->uv[w->f[i].f.x], w->vt[w->f[i].t.x]);
		F2COPY(w->uv[w->f[i].f.y], w->vt[w->f[i].t.y]);
		F2COPY(w->uv[w->f[i].f.z], w->vt[w->f[i].t.z]);
	}
	printf("UV's copied, wanted %d verts.\n", uvcopy);
}


static void wf_vbo_load(WF_OBJ *w)
{
#ifndef STATIC_TEST
	if(!w)return;

	w->vbo = 0;
	printf("genbuffers\n");
	printf("%s\n", glError(glGetError()));

	glGenBuffersARB(1, &w->vbo);
	printf("bindbuffer\n");
	glBindBufferARB(GL_ARRAY_BUFFER, w->vbo);
	printf("bufferdata\n");
	if(w->p)
	{
		glBufferDataARB(GL_ARRAY_BUFFER, w->nv*32, w->p, GL_STATIC_DRAW);
		glBindBufferARB(GL_ARRAY_BUFFER, 0);
		printf("faces too\n");
	}
	else
	{
		glBufferDataARB(GL_ARRAY_BUFFER, w->nv*12, w->v, GL_STATIC_DRAW);
		glBindBufferARB(GL_ARRAY_BUFFER, 0);
		printf("verts only\n");
		return;
	}

	glGenBuffersARB(1, &w->ebo);
	if(!w->ebo)printf("glGenBuffers failed\n");
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, w->ebo);
	printf("w->nf*12 = %d\n", w->nf*12);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER, w->nf*12, 0, GL_STATIC_DRAW);
	int *p = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	if(p)
	{
		memcpy(p, w->vf, w->nf*12);
	}
	else
	{
		printf("glMapBuffer() failed\n");
		printf("%s\n", glError(glGetError()));
	}
	glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER);
}


void wf_drawarray_points(WF_OBJ *w)
{
	if(!w)return;
	glBindBufferARB(GL_ARRAY_BUFFER, w->vbo);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 12, (GLvoid*)0);
	glDrawArrays(GL_POINTS, 0, w->nv);
	glBindBufferARB(GL_ARRAY_BUFFER, 0);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void wf_drawarray_triangles(WF_OBJ *w)
{
	if(!w)return;
	glBindBufferARB(GL_ARRAY_BUFFER, w->vbo);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(3,	GL_FLOAT, 32, (GLvoid*)0);
	glNormalPointer(GL_FLOAT, 32, (GLvoid*)12);
	glTexCoordPointer(2, GL_FLOAT, 32, (GLvoid*)24);

	GLint i=0;

	for(WF_MTL *m = w->m; m; m = m->next)
	{
		if(!m->nf)continue;
		mtl_begin(m);
		glDrawArrays(GL_TRIANGLES, i*3, m->nf*3);
		i += m->nf;
	}
	mtl_end();
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if(i<w->nf)
		glDrawArrays(GL_TRIANGLES, i*3, (w->nf-i)*3);

	glBindBufferARB(GL_ARRAY_BUFFER, 0);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
}

void wf_drawelements(WF_OBJ *w)
{
	if(!w)return;
	glBindBufferARB(GL_ARRAY_BUFFER, w->vbo);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_FLOAT, 32, (GLvoid*)0);
	glNormalPointer(GL_FLOAT, 32, (GLvoid*)12);
	glTexCoordPointer(2, GL_FLOAT, 32, (GLvoid*)24);

	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, w->ebo);

	WF_MTL *m = w->m;

	intptr_t i=0;
	while(m)
	{
		if(!m->nf)continue;
		mtl_begin(m);
		
	glDrawElements(GL_TRIANGLES, m->nf*12, GL_UNSIGNED_INT, (GLvoid*)(12*i));
		i += m->nf;
		m = m->next;
	}
	mtl_end();
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if(i<w->nf)
	glDrawElements(GL_TRIANGLES, (w->nf-i)*12,
			GL_UNSIGNED_INT, (GLvoid*)(i*12));

	glBindBufferARB(GL_ARRAY_BUFFER, 0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
#endif
}

WF_OBJ* wf_parse(char *filename)
{
	printf("Loading Wavefront OBJ(\"%s\");\n", filename);
	FILE *fptr = fopen(filename, "r");
	if(!fptr)return 0;

	char buf[1024];
	int i;


	WF_OBJ *w = malloc(sizeof(WF_OBJ));
	if(!w)
	{
		fclose(fptr);
		printf("wf_parse() malloc failed\n");
		return 0;
	}
	memset(w, 0, sizeof(WF_OBJ));
	w->filename = hcopy(filename);


	// count the verts
	while(fgets(buf, 1024, fptr))
	switch(buf[0]) {
	case 'm':
		tailchomp(buf);
		if(strstr(buf, "mtllib"))
			mtl_load(w, buf+7);
		break;
	case 'v':	// vertex data
		switch(buf[1]) {
		case ' ': w->nv++; break;
		case 'n': w->nn++; break;
		case 't': w->nt++; break;
		}
		break;
	case 'f':	// face
		tailchomp(buf);
		i = 2;
		for(; !eol(buf[i]) && ' ' != buf[i]; i++);
		if(' ' != buf[i])break;		// only a single index
		i++;
		for(; !eol(buf[i]) && ' ' != buf[i]; i++);
		if(' ' != buf[i])break;		// only a line
		i++;
		w->nf++;
		// every point after this is an extra triangle
		while(!eol(buf[i]))
		{		// I sure hope I only see convex polygons
			if(' ' == buf[i])
				w->nf++;
			i++;
		}
		break;
	case 'o':
	case 'g': w->ng++; break;
	case 's': w->ns++; break;
	}
	// alloc the memory
	if(w->nv)w->v = malloc(sizeof(float3)*w->nv);
	if(w->nt)w->vt = malloc(sizeof(float3)*w->nt);
	if(w->nn)w->vn = malloc(sizeof(float3)*w->nn);
	if(w->nf)
	{
		w->f = malloc(sizeof(WF_FACE)*w->nf);
		memset(w->f, 0, sizeof(WF_FACE)*w->nf);
	}
	if(w->ng)w->groups = malloc(sizeof(char*)*w->ng);
	if(w->ns)w->sgroups = malloc(sizeof(int)*w->ns);

	printf("Counted v=%d,t=%d,n=%d,f=%d,g=%d,s=%d,m=%d\n",
			w->nv, w->nt, w->nn, w->nf, w->ng, w->ns, w->nm);
	int vi=0, ti=0, ni=0, fi=0, gi=0, si=0;
	WF_MTL *mi=0;

	int first, last;
	int order=1;
	// read the verts
	rewind(fptr);
	int tmp;
	float3 *target;

	WF_MTL *lastmat=0;
	while(fgets(buf, 1024, fptr))
	{
		tailchomp(buf);
		int i=2;
//		printf("%s\n", buf);
		switch(buf[0]) {
		case 'v':

			switch(buf[1]) {
			default: //case ' ':
				target = &w->v[vi++];
				break;
			case 'n':
				target = &w->vn[ni++];
				i++;
				break;
			case 't': 
				target = &w->vt[ti++];
				i++;
				break;
			}

			while(' '==buf[i])i++;
			target->x = fast_atof(buf+i);
			while(' '!=buf[i])i++; i++;
			target->y = fast_atof(buf+i);
			if(buf[1]!='t')
			{
				while(' '!=buf[i])i++; i++;
				target->z = fast_atof(buf+i);
			}
			else
			{
				target->y = 1.0 - target->y;

			}
			break;
	
		case 'u':	
			tailchomp(buf);
			lastmat = find_material(w->m, buf+7);
			break;
		case 's':	// smooth group
			si = atoi(buf+2);

			break;
		case 'g':	// group - I haven't seen them together
		case 'o':	// object
			tailchomp(buf);
			w->groups[gi] = hcopy(buf+2);
			gi++;
			break;
		case 'f':
			i=2;
			for(; !eol(buf[i]) && ' ' != buf[i]; i++);
			if(' ' != buf[i])break;		// only a single index
			i++;
			for(; !eol(buf[i]) && ' ' != buf[i]; i++);
			if(' ' != buf[i])break;		// only a line
			i++;
			if(strstr(buf, "/"))order = 2;
			else order = 1;
			i=1;
			while(' '==buf[i])i++;	//find next space
			switch(order) {
			case 1:
				tmp = atoi(buf+i);
				if(tmp < 0)tmp += w->nv;
				else tmp--;
				w->f[fi].f.x = tmp;

//					printf("%d/", g->f[g->nf].x);
				while(' '!=buf[i])i++; i++;	//find next space
				tmp = atoi(buf+i);
				if(tmp < 0)tmp += w->nv;
				else tmp--;
				w->f[fi].f.y = tmp;
//					printf("%d/", g->f[g->nf].y);
				while(' '!=buf[i])i++; i++;
				tmp = atoi(buf+i);
				if(tmp < 0)tmp += w->nv;
				else tmp--;
				w->f[fi].f.z = tmp;
//					printf("%d\n", g->f[g->nf].z);
				w->f[fi].s = si;
				w->f[fi].m = lastmat;
				w->f[fi].g = gi;
				first = fi;
				last = first;
				fi++;
				// every point after this is an extra triangle
				while(!eol(buf[i]))
				{		// I sure hope I only see convex polygons
					if(' ' == buf[i])
					{
						i++;
						w->f[fi].f.x = w->f[first].f.x;
						w->f[fi].f.y = w->f[last].f.z;
						tmp = atoi(buf+i);
						if(tmp < 0) tmp += w->nv;
						else tmp--;
						w->f[fi].f.z = tmp;
				w->f[fi].s = si;
				w->f[fi].m = lastmat;
				w->f[fi].g = gi;


						last = fi;
						fi++;
					}
					i++;
				}
				break;
			case 2:
				tmp = atoi(buf+i);
				if(tmp < 0)tmp += w->nv;
				else tmp--;
				w->f[fi].f.x = tmp;
				while('/'!=buf[i])i++; i++;	//find next slash
				tmp = atoi(buf+i);
				if(tmp < 0)tmp += w->nt;
				else tmp--;
				w->f[fi].t.x = tmp;

//					printf("%d/", g->f[g->nf].x);
				while(' '!=buf[i])i++; i++;	//find next space
				tmp = atoi(buf+i);
				if(tmp < 0)tmp += w->nv;
				else tmp--;
				w->f[fi].f.y = tmp;
				while('/'!=buf[i])i++; i++;	//find next slash
				tmp = atoi(buf+i);
				if(tmp < 0)tmp += w->nt;
				else tmp--;
				w->f[fi].t.y = tmp;
//					printf("%d/", g->f[g->nf].y);
				while(' '!=buf[i])i++; i++;
				tmp = atoi(buf+i);
				if(tmp < 0)tmp += w->nv;
				else tmp--;
				w->f[fi].f.z = tmp;
				while('/'!=buf[i])i++; i++;	//find next slash
				tmp = atoi(buf+i);
				if(tmp < 0)tmp += w->nt;
				else tmp--;
				w->f[fi].t.z = tmp;
//					printf("%d\n", g->f[g->nf].z);
				w->f[fi].s = si;
				w->f[fi].m = lastmat;
				w->f[fi].g = gi;
				first = fi;
				last = first;
				fi++;
				// every point after this is an extra triangle
				while(!eol(buf[i]))
				{		// I sure hope I only see convex polygons
					if(' ' == buf[i])
					{
						i++;
						w->f[fi].f.x = w->f[first].f.x;
						w->f[fi].t.x = w->f[first].t.x;

						w->f[fi].f.y = w->f[last].f.z;
						w->f[fi].t.y = w->f[last].t.z;

						tmp = atoi(buf+i);
						if(tmp < 0) tmp += w->nv;
						else tmp--;
						w->f[fi].f.z = tmp;
						while('/'!=buf[i])i++; i++;	//find next slash
						tmp = atoi(buf+i);
						if(tmp < 0)tmp += w->nt;
						else tmp--;
						w->f[fi].t.z = tmp;

						w->f[fi].s = si;
						w->f[fi].m = lastmat;
						w->f[fi].g = gi;
						last = fi;
						fi++;
					}
					i++;
				}
				break;
		case 3:
				w->f[fi].f.x = abs(atoi(buf+i))-1;
				while('/'!=buf[i])i++; i++;
				if(buf[i]!='/')w->f[fi].t.x = abs(atoi(buf+i))-1;
//				while('/'!=buf[i])i++; i++;
//				if(buf[i]!=' ')w->f[fi].n.x = abs(atoi(buf+i))-1;

				while(' '!=buf[i])i++; i++;	//find next space
				w->f[fi].f.y = abs(atoi(buf+i))-1;
				while('/'!=buf[i])i++; i++;
				if(buf[i]!='/')w->f[fi].t.y = abs(atoi(buf+i))-1;
//				while('/'!=buf[i])i++; i++;
//				if(buf[i]!=' ')w->f[fi].n.y = abs(atoi(buf+i))-1;

				while(' '!=buf[i])i++; i++;	//find next space
				w->f[fi].f.z = abs(atoi(buf+i))-1;
				while('/'!=buf[i])i++; i++;
				if(buf[i]!='/')w->f[fi].t.z = abs(atoi(buf+i))-1;
//				while('/'!=buf[i])i++; i++;
//				if(buf[i]!=' ')w->f[fi].n.z = abs(atoi(buf+i))-1;
				
				w->f[fi].s = si;
				w->f[fi].m = mi;
				w->f[fi].g = gi;

				first = fi;
				last = first;
				fi++;
				// every point after this is an extra triangle
				while('\n'!=buf[i])
				{		// I sure hope I only see convex polygons
					if(' ' == buf[i])
					{
						i++;
						w->f[fi].f.x = w->f[first].f.x;
						w->f[fi].t.x = w->f[first].t.x;
						w->f[fi].n.x = w->f[first].n.x;
						w->f[fi].f.y = w->f[last].f.z;
						w->f[fi].t.y = w->f[last].t.z;
						w->f[fi].n.y = w->f[last].n.z;
				w->f[fi].f.z = abs(atoi(buf+i))-1;
				while('/'!=buf[i])i++; i++;
				if(buf[i]!='/')w->f[fi].t.z = abs(atoi(buf+i))-1;
//				while('/'!=buf[i])i++; i++;
//				if(buf[i]!=' ')w->f[fi].n.z = abs(atoi(buf+i))-1;
						w->f[fi].s = si;
						w->f[fi].m = mi;
						w->f[fi].g = gi;


						last = fi;
						fi++;
					}
					i++;
				}
				break;	// face reader break
			}
			break;	// line type break
		}
	}
	printf("Read    v=%d,t=%d,n=%d,f=%d,g=%d,s=%d\n",
			vi, ti, ni, fi, gi, si);

	fclose(fptr);
	printf("At fclose we have %d/%d/%d\n", w->nv, w->nt, w->nn);
	return w;
}


WF_OBJ* wf_load(char * filename)
{
	WF_OBJ *w = wf_parse(filename);

	wf_bound(w);
	if(w->nv != w->nn)wf_normals(w);
	wf_texcoords(w);

	if(!w->nf)
	{
		w->draw = wf_drawarray_points;

		glGenBuffersARB(1, &w->vbo);
		glBindBufferARB(GL_ARRAY_BUFFER, w->vbo);
		glBufferDataARB(GL_ARRAY_BUFFER, w->nv*12, w->v, GL_STATIC_DRAW);
		glBindBufferARB(GL_ARRAY_BUFFER, 0);
		printf("verts only\n");
	}
	else
	if(GLEW_VERSION_1_5 && GLEW_ARB_vertex_buffer_object)
	{
		wf_interleave(w);
		wf_vbo_load(w);

		w->draw = wf_drawelements;
	}
	else
	{
		wf_buffer_drawarray(w);
		w->draw = wf_drawarray_triangles;
		glGenBuffersARB(1, &w->vbo);
		glBindBufferARB(GL_ARRAY_BUFFER, w->vbo);
		glBufferDataARB(GL_ARRAY_BUFFER, w->nf*32*3, w->p, GL_STATIC_DRAW);
		glBindBufferARB(GL_ARRAY_BUFFER, 0);
	}
	printf("All done.\n");
	return w;
}


void wf_free(WF_OBJ *w)
{
	if(!w)return;
	if(w->v)free(w->v);
	if(w->vt)free(w->vt);
	if(w->vn)free(w->vn);
	if(w->p)free(w->p);
	if(w->f)free(w->f);
	if(w->fn)free(w->fn);
#ifndef STATIC_TEST
	if(w->vbo)glDeleteBuffersARB(1, &w->vbo);
	if(w->ebo)glDeleteBuffersARB(1, &w->ebo);
#endif
	WF_MTL *mt, *m = w->m;
	while(m)
	{
		mt = m->next;
		mtl_free(m);
		m = mt;
	}
	if(w->filename)free(w->filename);
	free(w); 
}

#ifdef STATIC_TEST

int main(int argc, char *argv[])
{
	WF_OBJ *w;
	switch(argc) {
		case 2:
			w = wf_load(argv[1]);
			wf_free(w);
			break;
		default:
			printf("user error.\n");
			return 1;
	}

	return 0;
}

#endif /* STATIC_TEST */

