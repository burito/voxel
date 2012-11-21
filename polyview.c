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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GL/glew.h>

#include "main.h"

typedef struct VERT
{
	float x,y,z;
} VERT;

typedef struct FACE
{
	int x, y, z;
} FACE;


typedef struct MESH
{
	int nv, nn, nf;
	VERT *v, *n;
	FACE *f;
} MESH;

MESH* mesh_load(char *filename)
{
	int i;
	MESH *m = malloc(sizeof(MESH));
	FILE *fptr = fopen(filename, "rb");
	char magic[] = "ERRR";
//	int zero = 0;
	fread(magic, 4, 1, fptr);
	// check the magic bytes?

	fread(&m->nv, 4, 1, fptr);
	fread(&m->nf, 4, 1, fptr);
	fread(&m->nn, 4, 1, fptr);

	m->v = malloc(sizeof(VERT)*m->nv);
	m->f = malloc(sizeof(FACE)*m->nf);
	m->n = malloc(sizeof(VERT)*m->nn);

	for(i=0; i<m->nv; i++)
	{
		fread(&m->v[i].x, 4, 1, fptr);
		fread(&m->v[i].y, 4, 1, fptr);
		fread(&m->v[i].z, 4, 1, fptr);
//		fread(&zero, 4, 1, fptr);
		fread(&m->n[i].x, 4, 1, fptr);
		fread(&m->n[i].y, 4, 1, fptr);
		fread(&m->n[i].z, 4, 1, fptr);
//		fread(&zero, 4, 1, fptr);
	}

	for(i=0; i<m->nf; i++)
	{
		fread(&m->f[i].x, 4, 1, fptr);
		fread(&m->f[i].y, 4, 1, fptr);
		fread(&m->f[i].z, 4, 1, fptr);
	}

	return m;
}


typedef struct VBO
{
	GLuint vbo, ebo;
	int svbo, sebo;
	void *buf_vbo, *buf_ebo;

} VBO;

VBO* vbo_load(char *filename)
{
	VBO *obj = malloc(sizeof(VBO));
	FILE *fptr = fopen(filename, "rb");
	char magic[] = "ERRR";

	fread(magic, 4, 1, fptr);
	// check the magic bytes?
	fread(&obj->svbo, 4, 1, fptr);
	fread(&obj->sebo, 4, 1, fptr);
	fread(magic, 4, 1, fptr);
	
	obj->buf_vbo = malloc(obj->svbo*24);
	fread(obj->buf_vbo, obj->svbo*24, 1, fptr);
	obj->buf_ebo = malloc(obj->sebo*12);
	fread(obj->buf_ebo, obj->sebo*12, 1, fptr);
	fclose(fptr);

	glGenBuffers(1, &obj->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
	glBufferData(GL_ARRAY_BUFFER, obj->svbo*24, obj->buf_vbo, GL_STATIC_DRAW);


	glGenBuffers(1, &obj->ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, obj->sebo*12, obj->buf_ebo, GL_STATIC_DRAW);
	int *p = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	memcpy(p, obj->buf_ebo, obj->sebo);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return obj;
}


void vbo_draw(VBO *obj)
{
	glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
	glVertexPointer(3, GL_FLOAT, 24, (GLvoid*)0);
	glNormalPointer(GL_FLOAT, 24, (GLvoid*)12);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
//	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->ebo);

//	glDrawElements(GL_TRIANGLES, obj->sebo*12, GL_UNSIGNED_INT, (GLvoid*)0);
	glDrawArrays(GL_POINTS, 0, obj->svbo);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
//	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


VBO *foo;

int main_init(int argc, char *argv[])
{
//	for( int i=0; i<argc; i++)
//		printf("argv[%d]=\"%s\"\n", i, argv[i]);

	if(argc != 2)
		return 1;
	foo = vbo_load(argv[1]);
	return 0;
}


float rx=0, ry=0, rz=0;
float px=0, py=0, pz=0;


void main_loop(void)
{
	if(keys[KEY_ESCAPE])
		killme=1;
	if(mouse[0])
	{
		rx += mickey_x * 0.1;
		ry += mickey_y * 0.1;
	}
	if(mouse[1])
	{
		px += mickey_x *0.001;
		py += mickey_y *0.001;
	}

	glClearDepth(5000.0f);
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//	glEnable(GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float ysize = (float)vid_height/(float)vid_width;

	glOrtho(0, 1.0, 0, ysize, -1, 1000);

	GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat mat_shininess[] = { 50.0 };
	GLfloat light_position[] = { 1.0, 1.0, -100.0, 0.0 };
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);
//	glEnable(GL_CULL_FACE);
//	glFrontFace(GL_CCW);
//	gluPerspective(30.0f,(GLfloat)vid_width/(GLfloat)vid_height,1.0f,5000.0f);


//	glOrtho(0, vid_width, 0, vid_height, 1, 10000 );

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
//	glTranslatef(px,py, -200.0f);
	glTranslatef(0.5,0.5, 0.5f);
	glRotatef(rx, 1.0f, 0.0f, 0.0f);
	glRotatef(ry, 0.0f, 1.0f, 0.0f);
	glRotatef(rz, 0.0f, 0.0f, 1.0f);
	glTranslatef(-0.5,-0.5, -0.5f);

	glColor4f(1.0, 1.0, 1.0, 1.0);

	
	vbo_draw(foo);
/*
	for( int i=0; i< obj->nf; i++)
	{
		glBegin(GL_TRIANGLES);
		glNormal3fv(&obj->n[obj->f[i].x]);
		glVertex3fv(&obj->v[obj->f[i].x]);
		glNormal3fv(&obj->n[obj->f[i].y]);
		glVertex3fv(&obj->v[obj->f[i].y]);
		glNormal3fv(&obj->n[obj->f[i].z]);
		glVertex3fv(&obj->v[obj->f[i].z]);
		glEnd();
	}
*/
}


void main_end(void)
{


}


