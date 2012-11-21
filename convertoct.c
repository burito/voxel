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

#include <zlib.h>

#include "3dmaths.h"


typedef struct MESH
{
	unsigned int nv, nn, nf;
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

typedef struct ListInt {
	int x;
	struct ListInt *next;
} ListInt;

typedef struct ListInt2 {
	int x, y;
	struct ListInt2 *next;
} ListInt2;


void gen_vertex_normals(MESH *m)
{

	ListInt *vert = malloc(sizeof(ListInt)*m->nv);
	memset(vert, 0, sizeof(ListInt)*m->nv);


	// tell each vert about it's faces
	for(int i=0; i<m->nf; i++)
	{
		int index = m->f[i].x;
		ListInt *tmp = malloc(sizeof(ListInt));
		tmp->x = i;
		tmp->next = vert[index].next;
		vert[index].next = tmp;

		index = m->f[i].y;
		tmp = malloc(sizeof(ListInt));
		tmp->x = i;
		tmp->next = vert[index].next;
		vert[index].next = tmp;

		index = m->f[i].z;
		tmp = malloc(sizeof(ListInt));
		tmp->x = i;
		tmp->next = vert[index].next;
		vert[index].next = tmp;
	}

	float3 *no = malloc(sizeof(float3)*m->nv);	// normal out
	// now average the normals and store in the output
	for(int i=0; i<m->nv; i++)
	{
		ListInt *tmp = vert[i].next;
		float3 t = { 0, 0, 0 };

		while(tmp)
		{
			vect_add( &t, &t, &m->n[tmp->x]);
			ListInt *last = tmp;
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


typedef struct OctItem {
	unsigned int child;
	unsigned int brick;
} OctItem;

typedef struct OctBlock {
	OctItem n[8];
} OctBlock;

typedef struct OctBrick {
	int nv, nf;
	ListInt2 *v;
	ListInt2 *f;
} OctBrick;

typedef struct OctTree {
	int blockcount, brickcount;
	int BlockPoolSize, BrickPoolSize;
	OctBlock *block;
	OctBrick *brick;
	float4 *location;
} OctTree;


#define WIGGLE 0.0
int inside(float3 *pos, const float4 *volume)
{
	if(pos->x >= volume->x-WIGGLE && pos->x < volume->x+volume->w+WIGGLE)
	if(pos->y >= volume->y-WIGGLE && pos->y < volume->y+volume->w+WIGGLE)
	if(pos->z >= volume->z-WIGGLE && pos->z < volume->z+volume->w+WIGGLE)
		return 1;
	return 0;
}


OctTree* oct_new(int BlockPoolSize, int BrickPoolSize)
{
	OctTree *ret = malloc(sizeof(OctTree));
	ret->block = malloc(sizeof(OctBlock)*BlockPoolSize);
	ret->brick = malloc(sizeof(OctBrick)*BrickPoolSize);
	ret->location = malloc(sizeof(float4)*BrickPoolSize);
	memset(ret->block, 0, sizeof(OctBlock)*BlockPoolSize);
	memset(ret->brick, 0, sizeof(OctBrick)*BrickPoolSize);
	memset(ret->location, 0, sizeof(float4)*BrickPoolSize);
	ret->blockcount = 1;
	ret->brickcount = 1;
	ret->BlockPoolSize = BlockPoolSize;
	ret->BrickPoolSize = BrickPoolSize;
	return ret;
}

// allocate a new block from the LRU...
int oct_new_block(OctTree *tree)
{
//	tree->blockcount++;
	if(tree->blockcount >= tree->BlockPoolSize)
	{
		printf("Overshot the block pool.\n");
		return 0;
	}
//	memset(&tree->block[tree->blockcount], 0, sizeof(OctBlock));
	return tree->blockcount++;
}

// allocate a new brick from the LRU...
int oct_new_brick(OctTree *tree, float4 *volume)
{
//	tree->brickcount++;
	if(tree->brickcount >= tree->BrickPoolSize)
	{
		printf("Overshot the brick pool.\n");
		return 0;
	}

	F4COPY(tree->location[tree->brickcount], (*volume));

//	tree->location[tree->brickcount].x = volume->x;
//	tree->location[tree->brickcount].y = volume->y;
//	tree->location[tree->brickcount].z = volume->z;
//	tree->location[tree->brickcount].w = volume->w;

//	memset(&tree->brick[tree->brickcount], 0, sizeof(OctBrick));
	return tree->brickcount++;
}

int oct_child(float3 *pos, float4 *vol)
{
	int child=0;
	vol->w *= 0.5;
	float3 mid;
	vect_sadd(&mid, (float3*)vol, vol->w);
	if(pos->x >= mid.x)
	{
		vol->x +=vol->w;
		child += 1;
	}

	if(pos->y >= mid.y)
	{
		vol->y += vol->w;
		child += 2;
	}

	if(pos->z >= mid.z)
	{
		vol->z += vol->w;
		child += 4;
	}
	return child;
}


int oct_leaf(OctTree *tree, float3 *pos, int depth)
{
	float4 volume = {0.0f, 0.0f, 0.0f, 1.0f};
	int this = 0;
	int child=0;

	for(int i=0; i<depth; i++)
	{
		child = oct_child(pos, &volume);

		if(tree->block[this].n[child].child )
		{
			this = tree->block[this].n[child].child;
		}
		else
		{
			int tmp = oct_new_block(tree);
			tree->block[this].n[child].child = tmp;
			this = tmp;
		}
	}

	child = oct_child(pos, &volume);
	if(!tree->block[this].n[child].brick)
	{
		tree->block[this].n[child].brick = oct_new_brick(tree, &volume);
	}

	return tree->block[this].n[child].brick;
}


int oct_read(OctTree *tree, float3 *pos)
{
	float4 volume = {0.0f, 0.0f, 0.0f, 1.0f};

	if(!inside(pos, &volume))
		return 0;

	int this = 0;

	int child=0;
	while(1)
	{
		child = oct_child(pos, &volume);

		if(tree->block[this].n[child].child )
		{
			this = tree->block[this].n[child].child;
		}
		else
		{
			if(!tree->block[this].n[child].brick)
			{
		tree->block[this].n[child].brick = oct_new_brick(tree, &volume);

			}
			return tree->block[this].n[child].brick;
		}
	}
}

#define B_SIZE 8
#define B_COUNT 48
#define B_EDGE (B_SIZE*B_COUNT)
#define B_CUBE (B_EDGE*B_EDGE*B_EDGE)



void gen_oct(MESH *m, char *filename)
{
	// place each point into the oct-tree
	OctTree *tree = oct_new(100000, 500000);
	int depth = 6;
	float total = (float)(B_EDGE-1) / (float)B_EDGE;
	
	for(int i=0; i<m->nv; i++)
	{
		F3MULS(m->v[i], m->v[i], total);
		int leaf = oct_leaf(tree, &m->v[i], depth);
		ListInt2 *tmp = malloc(sizeof(ListInt2));
		tmp->x = i;
		tmp->y = tree->brick[leaf].nv;
		tmp->next = tree->brick[leaf].v;
		tree->brick[leaf].v = tmp;
		tree->brick[leaf].nv++;
	}
	depth++;

	printf("TreeBlock/Brick = (%d %d)\n",
			tree->blockcount, tree->brickcount);

//	tree->brickcount ++;
	unsigned int buf_size = 16*B_CUBE;
	float4 (*bricktex)[B_EDGE][B_EDGE] = malloc(buf_size);
	memset(bricktex, 0, buf_size);
	int3 index;
	int3 itmp;
	int id, idt;
	ListInt2 *tmp;


	printf("Generating Voxel bricks.\n");
	int lastbrickcount = tree->brickcount;
	for(int i=0; i<lastbrickcount; i++)
	{
//		printf("Brick #%d. %d\n", i, tree->brick[i].nv);
		tmp = tree->brick[i].v;
		if(tmp)
		for(int j=0; j<tree->brick[i].nv && tmp; j++) //while(tmp)
		{
			index.x = ((int)(m->v[tmp->x].x * (7<<depth)) % (B_SIZE-1))+1;
			index.y = ((int)(m->v[tmp->x].y * (7<<depth)) % (B_SIZE-1))+1;
			index.z = ((int)(m->v[tmp->x].z * (7<<depth)) % (B_SIZE-1))+1;
			id = i;
			itmp.x = id % B_COUNT;
			idt = (id-itmp.x) / B_COUNT;
			itmp.y = idt % B_COUNT;
			itmp.z = (idt-itmp.y) / B_COUNT;
			F3MULS(itmp, itmp, (B_SIZE));
			F3ADD(index, index, itmp);
			F3COPY(bricktex[index.z][index.y][index.x], m->n[tmp->x]);
			bricktex[index.z][index.y][index.x].w = 1.0f;
			tmp = tmp->next;
		}

	}

	printf("Copy neighbour voxels.\n");
	for(int i=1; i<tree->brickcount; i++)
	{
		float size = tree->location[i].w;
		int3 d,s;
		d.x = i % B_COUNT;
		idt = (i-d.x) / B_COUNT;
		d.y = idt % B_COUNT;
		d.z = (idt-d.y) / B_COUNT;
		F3MULS(d, d, B_SIZE);

		int src;
		float3 pos;

		F3COPY(pos, tree->location[i]);
		pos.x += size*1.5;
		pos.y += size * 0.5;
		pos.z += size * 0.5;
		src = oct_read(tree, &pos);
		if(src)
		{
			s.x = src % B_COUNT;
			idt = (src-s.x) / B_COUNT;
			s.y = idt % B_COUNT;
			s.z = (idt-s.y) / B_COUNT;
			F3MULS(s, s, B_SIZE);

			for(int x=0; x<B_SIZE; x++)
			for(int y=0; y<B_SIZE; y++)
			{
		bricktex[s.z+x][s.y+y][s.x] = bricktex[d.z+x][d.y+y][d.x+7];
			}
		}

		F3COPY(pos, tree->location[i]);
		pos.x += size*0.5;
		pos.y += size*1.5;
		pos.z += size*0.5;
		src = oct_read(tree, &pos);
		if(src)
		{
			s.x = src % B_COUNT;
			idt = (src-s.x) / B_COUNT;
			s.y = idt % B_COUNT;
			s.z = (idt-s.y) / B_COUNT;
			F3MULS(s, s, B_SIZE);

			for(int x=1; x<B_SIZE; x++)
			for(int y=1; y<B_SIZE; y++)
			{
		bricktex[s.z+x][s.y][s.x+y] = bricktex[d.z+x][d.y+7][d.x+y];
			}
		}

		F3COPY(pos, tree->location[i]);
		pos.x += size*0.5;
		pos.y += size*0.5;
		pos.z += size*1.5;
		src = oct_read(tree, &pos);
		if(src)
		{
			s.x = src % B_COUNT;
			idt = (src-s.x) / B_COUNT;
			s.y = idt % B_COUNT;
			s.z = (idt-s.y) / B_COUNT;
			F3MULS(s, s, B_SIZE);

			for(int x=1; x<B_SIZE; x++)
			for(int y=1; y<B_SIZE; y++)
			{
			bricktex[s.z][s.y+x][s.x+y] = bricktex[d.z+7][d.y+x][d.x+y];
			}
		}


		F3COPY(pos, tree->location[i]);
		pos.x += size*1.5;
		pos.y += size*1.5;
		pos.z += size*0.5;
		src = oct_read(tree, &pos);
		if(src)
		{
			s.x = src % B_COUNT;
			idt = (src-s.x) / B_COUNT;
			s.y = idt % B_COUNT;
			s.z = (idt-s.y) / B_COUNT;
			F3MULS(s, s, B_SIZE);

			for(int x=1; x<B_SIZE; x++)
			{
			bricktex[s.z+x][s.y][s.x] = bricktex[d.z+x][d.y+7][d.x+7];
			}
		}


		F3COPY(pos, tree->location[i]);
		pos.x += size*1.5;
		pos.y += size*0.5;
		pos.z += size*1.5;
		src = oct_read(tree, &pos);
		if(src)
		{
			s.x = src % B_COUNT;
			idt = (src-s.x) / B_COUNT;
			s.y = idt % B_COUNT;
			s.z = (idt-s.y) / B_COUNT;
			F3MULS(s, s, B_SIZE);

			for(int x=1; x<B_SIZE; x++)
			{
			bricktex[s.z][s.y+x][s.x] = bricktex[d.z+7][d.y+x][d.x+7];
			}
		}

		F3COPY(pos, tree->location[i]);
		pos.x += size*0.5;
		pos.y += size*1.5;
		pos.z += size*1.5;
		src = oct_read(tree, &pos);
		if(src)
		{
			s.x = src % B_COUNT;
			idt = (src-s.x) / B_COUNT;
			s.y = idt % B_COUNT;
			s.z = (idt-s.y) / B_COUNT;
			F3MULS(s, s, B_SIZE);

			for(int x=1; x<B_SIZE; x++)
			{
			bricktex[s.z][s.y][s.x+x] = bricktex[d.z+7][d.y+7][d.x+x];
			}
		}

		F3COPY(pos, tree->location[i]);
		pos.x += size*1.5;
		pos.y += size*1.5;
		pos.z += size*1.5;
		src = oct_read(tree, &pos);
		if(src)
		{
			s.x = src % B_COUNT;
			idt = (src-s.x) / B_COUNT;
			s.y = idt % B_COUNT;
			s.z = (idt-s.y) / B_COUNT;
			F3MULS(s, s, B_SIZE);

			bricktex[s.z][s.y][s.x] = bricktex[d.z+7][d.y+7][d.x+7];
		}

	}
	printf("TreeBlock/Brick = (%d %d)\n",
			tree->blockcount, tree->brickcount);
	printf("Walking the normals.\n");
	for(int i=1; i<tree->brickcount; i++)
	{
		

	}

	printf("Writing \"%s\".\n", filename);
	FILE *fptr = fopen(filename, "wb");
	int zero = 0;
	fwrite("VOCT", 4, 1, fptr);
	fwrite(&tree->blockcount, 4, 1, fptr);
	fwrite(&tree->brickcount, 4, 1, fptr);
	fwrite(&zero, 4, 1, fptr);
	fwrite(tree->block, tree->blockcount, sizeof(OctBlock), fptr);

	z_stream strm = { .zalloc=Z_NULL, .zfree=Z_NULL, .opaque=Z_NULL };
	int ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
	if(ret != Z_OK)
	{
		printf("deflateInit() failed.\n");
		return;
	}
#define CHUNK (256 * 1024)
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];
	int row = sizeof(float4)*B_SIZE;

	int have, inoff = 0;
	for(int i=0; i<tree->brickcount; i++)
	{
		int3 boff;
		int itmp = i % B_COUNT;
		boff.x = itmp;
		itmp = (i-boff.x) / B_COUNT;
		boff.y = itmp % B_COUNT;
		boff.z = (itmp-boff.y) / B_COUNT;
		F3MULS(boff, boff, B_SIZE);
		for(int z=0; z < B_SIZE; z++)
		for(int y=0; y < B_SIZE; y++)
		{
			memcpy(in+inoff, &bricktex[boff.z+z][boff.y+y][boff.x], row);
			inoff += row;
			if(inoff+row > CHUNK)
			{
				strm.next_in = in;
				strm.avail_in = inoff;
				do{
					strm.avail_out = CHUNK;
					strm.next_out = out;
					ret = deflate(&strm, Z_NO_FLUSH);
//					assert(ret != Z_STREAM_ERROR);
					have = CHUNK - strm.avail_out;
					fwrite(out, have, 1, fptr);
				} while (strm.avail_out == 0);
//				assert(strm.avail_in == 0);
				inoff = 0;
			}
//		fwrite(&bricktex[boff.z+z][boff.y+y][boff.x],
//				sizeof(float4)*B_SIZE, 1, fptr);
		}
	}
	strm.next_in = in;
	strm.avail_in = inoff;
	do{
		strm.avail_out = CHUNK;
		strm.next_out = out;
		ret = deflate(&strm, Z_FINISH);
//		assert(ret != Z_STREAM_ERROR);
		have = CHUNK - strm.avail_out;
		fwrite(out, have, 1, fptr);
	} while (strm.avail_out == 0);
//	assert(strm.avail_in == 0);
	deflateEnd(&strm);

//	fwrite(bricktex, 16*B_CUBE, 1, fptr);
//	fwrite(bricktex[B_EDGE], 16*B_CUBE, 1, fptr);
	fclose(fptr);
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

MESH* mesh_load(char *filename)
{
	int i;
	MESH *m = malloc(sizeof(MESH));
	FILE *fptr = fopen(filename, "rb");
	char magic[] = "ERRR";
	fread(magic, 4, 1, fptr);
	// check the magic bytes?

	fread(&m->nv, 4, 1, fptr);
	fread(&m->nf, 4, 1, fptr);
	fread(&m->nn, 4, 1, fptr);
	m->nn = m->nv;

	m->v = malloc(sizeof(float3)*m->nv);
	m->f = malloc(sizeof(int3)*m->nf);
	m->n = malloc(sizeof(float3)*m->nn);

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
	fclose(fptr);
	return m;
}


int main(int argc, char *argv[])
{
	MESH *m;
	switch(argc) {
		case 3:
			m = mesh_load(argv[1]);
			printf("Beginning OctBuild.\n");
			gen_oct(m, argv[2]);
			printf("OctBuild Completed.\n");

			break;
		default:
			printf("user error.\n");
			return 1;
	}

	return 0;
}
/*
typedef struct GigaNode {
	int todo;
} GigaNode;

typedef struct GigaBrick {
	int todo;
} GigaBrick;


typedef struct GigaOct {

	int NodePTSize;
	int *NodePT;
	int NodePoolSize;
	GigaNode *NodePool;
	int *NodeRequest;
	int2 *NodeUsage;	// x = pool, y = PT
	int *NodeLRU;
	int *NodeLocal;

	int BrickPTSize;
	int *BrickPT;
	int BrickPoolSize;
	GigaBrick *BrickPool;
	int *BrickRequest;
	int2 *BrickUsage;	// x = pool, y = PT
	int *BrickLRU;
	int *BrickLocal;

} GigaOct;
*/

