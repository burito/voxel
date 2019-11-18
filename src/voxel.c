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
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/glew.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define radians(X) ((3.14159265358979323846/180.0)*X)

#include <zlib.h>
#include "global.h"
#include "shader.h"
#include "3dmaths.h"


void prepare_shader(void);
void free_vram(void);

#define B_SIZE 8
#define B_COUNT 48
#define B_EDGE (B_SIZE*B_COUNT)
#define B_CUBE (B_EDGE*B_EDGE*B_EDGE)
#define N_SIZE 100000

typedef struct NodeItem {
	unsigned int child;
	unsigned int brick;
} NodeItem;

typedef struct NodeBlock {
	NodeItem n[8];
} NodeBlock;

GLuint vert, frag, prog;
GLuint node_pool;
GLuint brick_tex;

NodeBlock *nPool = NULL;
float4 (*bPool)[B_EDGE][B_EDGE] = NULL;
int bPool_size = B_CUBE * sizeof(float4);
int nPool_size = N_SIZE * 64;
unsigned int now = 1;
float fov = 90.0;

GLint s_fovdepth, s_pixelarc;
GLint s_pos, s_angle;
GLint s_nPool, s_bPool; //, s_now;
GLuint64EXT nodepool_devptr;

int brick_count = 0;


void bp_add(float4 *data)
{
	int doff = 0;
	int3 boff;
	int itmp = brick_count % B_COUNT;
	boff.x = itmp;
	itmp = (brick_count-boff.x)/B_COUNT;
	boff.y = itmp % B_COUNT;
	boff.z = (itmp-boff.y) / B_COUNT;
	boff = mul(boff, B_SIZE);
	for(int z=0; z < B_SIZE; z++)
	for(int y=0; y < B_SIZE; y++)
	{
		memcpy(&bPool[boff.z+z][boff.y+y][boff.x],
			data+doff, sizeof(float4)*B_SIZE);
		doff += B_SIZE;
	}
	brick_count++;
}

#define CHUNK (256 * 1024)
int voxel_zread(int bricks, FILE * fptr)
{
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	z_stream strm = {
			.zalloc=Z_NULL,
			.zfree=Z_NULL,
			.opaque=Z_NULL,
			.avail_in=0,
			.next_in=Z_NULL
	};
	int ret = inflateInit(&strm);
	if(ret != Z_OK)
		return ret;

	int prev_off = 0;
	int brick_size = B_SIZE*B_SIZE*B_SIZE*sizeof(float4);
	unsigned char *prev_buf = malloc(brick_size);
		
	do {
		strm.avail_in = fread(in, 1, CHUNK, fptr);
		if(ferror(fptr)) {
			ret = Z_ERRNO;
			goto zread_fail;
		}
		if(strm.avail_in == 0)
			break;
		strm.next_in = in;

		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
//			assert(ret != Z_STREAM_ERROR);
			switch(ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				goto zread_fail;
			}
			int have = CHUNK - strm.avail_out;
// pass data to loader
			int outoff = 0; 
			if(have+prev_off > brick_size)
			{
				if(prev_off)
				{
					outoff = brick_size - prev_off;
					memcpy(prev_buf+prev_off, out, outoff);
					bp_add((float4*)prev_buf);
					prev_off = 0;
				}
				for(;outoff+brick_size <= have; outoff+= brick_size)
					bp_add((float4*)(out+outoff));
			}
			if(outoff < have)
			{
				memcpy(prev_buf+prev_off, out+outoff, have - outoff);
				prev_off += have - outoff;
				if(prev_off >= brick_size)
				{
					bp_add((float4*)prev_buf);
					prev_off = 0;
				}
			}
	//		strm.avail_out = 0;
// end data loader

		} while(strm.avail_out == 0);
	} while(ret != Z_STREAM_END);

	inflateEnd(&strm);
	printf("zlib ok: inflated %d bricks.\n", brick_count-1);
	return 0;
zread_fail:
	inflateEnd(&strm);
	free(prev_buf);
	printf("zlib fail = %d, bricks=%d\n", ret, brick_count);
	return ret;
}


int voxel_init(char *filename)
{

	bPool = malloc(bPool_size);
	memset(bPool, 0, bPool_size);
	nPool = malloc(nPool_size);
	memset(nPool, 0, nPool_size);
	// Load file off of disk
	FILE *fptr = fopen(filename, "rb");
	if(!fptr)
	{
		printf("Failed Opening SVO file \"%s\"\n", filename);
		return 0;
	}


	char check[4] = "VOCT";
	int bricks, blocks;
	int zero;

	fread(check, 4, 1, fptr);
	fread(&blocks, 4, 1, fptr);
	fread(&bricks, 4, 1, fptr);
	fread(&zero, 4, 1, fptr);

	fread(nPool, blocks * 64, 1, fptr);
	int ret = voxel_zread(bricks, fptr);

	if(ret)
	{
		printf("zload failed\n");

	}
/*
	for(int i=0; i < bricks; i++)
	{
		int3 boff;
		int itmp = i % B_COUNT;
		boff.x = itmp;
		itmp = (i-boff.x)/B_COUNT;
		boff.y = itmp % B_COUNT;
		boff.z = (itmp-boff.y) / B_COUNT;
		F3MULS(boff, boff, B_SIZE);
		for(int z=0; z < B_SIZE; z++)
		for(int y=0; y < B_SIZE; y++)
		{
			
	fread(&bPool[boff.z+z][boff.y+y][boff.x],
			sizeof(float4)*B_SIZE, 1, fptr);
		}
		
	}
*/
//	fread(bPool, 16*B_CUBE, 1, fptr);
	fclose(fptr);
	
	printf("Loaded %d blocks, %d bricks\n", blocks, bricks);
	printf("Error = \"%s\"\n", glError(glGetError()));

	free_vram();

	// Send the brick pool to the GPU
	glEnable(GL_TEXTURE_3D);
	glGenTextures(1, &brick_tex);
	glBindTexture(GL_TEXTURE_3D, brick_tex);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	printf("Error = \"%s\"\n", glError(glGetError()));
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F,
			B_EDGE, B_EDGE, B_EDGE,
			0, GL_RGBA, GL_FLOAT, bPool);
	printf("Error = \"%s\"\n", glError(glGetError()));
	glBindTexture(GL_TEXTURE_3D, 0);

	printf("Error = \"%s\"\n", glError(glGetError()));
	free_vram();

	// load the node pool onto the GPU
	glGenBuffers(1, &node_pool);
	glBindBuffer(GL_ARRAY_BUFFER, node_pool);
	glBufferData(GL_ARRAY_BUFFER, nPool_size, nPool, GL_DYNAMIC_DRAW);
	glGetBufferParameterui64vNV(GL_ARRAY_BUFFER,
			GL_BUFFER_GPU_ADDRESS_NV, &nodepool_devptr);
	glMakeBufferResidentNV(GL_ARRAY_BUFFER, GL_READ_ONLY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	printf("Error = \"%s\"\n", glError(glGetError()));
	prepare_shader();
	return 1;
}


void prepare_shader(void)
{
	// build the shader program
	vert = shader_load(GL_VERTEX_SHADER, "data/render.vert");
	frag = shader_load(GL_FRAGMENT_SHADER, "data/render.frag");
	prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);
	GLint param;
	glGetProgramiv(prog, GL_LINK_STATUS, &param);
	printf("*** Shader linking %s.\n",
		param == GL_FALSE ? "went as expected" : "worked");
	printProgramInfoLog(prog);

	// prepare uniforms etc for the shader	
	s_angle = glGetUniformLocation(prog, "angle");	
	s_pos = glGetUniformLocation(prog, "position");	
	s_pixelarc = glGetUniformLocation(prog, "pixel_arc");
	s_fovdepth = glGetUniformLocation(prog, "fovdepth");
	s_bPool = glGetUniformLocation(prog, "bPool");	
	s_nPool = glGetUniformLocation(prog, "nPool");	
//	s_now = glGetUniformLocation(prog, "now");	
//	glUniform1i(s_now, now);
	glUniformui64NV(s_nPool, nodepool_devptr);
//	glUniform1i(s_bPool, 0);

//	glMakeBufferNonResidentNV(GL_ARRAY_BUFFER_ARB);
}

extern vect pos;
extern vect angle;




void voxel_draw(void)
{
	if(keys[KEY_R])
	{
		keys[KEY_R] = 0;
		printf("Reloading shader.\n");
		glDeleteShader(vert);
		glDeleteShader(frag);
		glDeleteProgram(prog);
		prepare_shader();
	}


//	killme=1;

//	vect pos, normal;

//	pos.x = 0.5 + sin(px)*pz;
//	pos.y = 0.5;
//	pos.z = 0.5 + cos(px)*pz;

//	pos.x = px; pos.y = py; pos.z = pz;

//	glUseProgram(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, brick_tex);
	glBindBuffer(GL_ARRAY_BUFFER, node_pool);
	glUseProgram(prog);
//	glUniform1f(s_fovdepth, tan(radians(fov)*0.5)*0.5);
//	glUniform1f(s_width, sin(radians(fov)/(float)vid_width));
	glUniform3f(s_angle, angle.x, angle.y, angle.z);
	glUniform3f(s_pos, pos.x, pos.y, pos.z);
	glUniformui64NV(s_nPool, nodepool_devptr);

//	glTranslatef(0, ((float)vid_height/(float)vid_width)*0.5-0.5 , 0);

	glBegin(GL_QUADS);
	glNormal3f(0,0,-1);
//	glColor3f(0,0,1);
	glTexCoord3f(0,0,0);
	glVertex3f(0,0,0);
//	glColor3f(1,0,1);
	glTexCoord3f(1,0,0);
	glVertex3f(1,0,0);
//	glColor3f(1,1,1);
	glTexCoord3f(1,1,0);
	glVertex3f(1,1,0);
//	glColor3f(0,1,1);
	glTexCoord3f(0,1,0);
	glVertex3f(0,1,0);
	glEnd();
	glUseProgram(0);
	now ++;
	// everyone in /r/opengl is crying at this point.
}

void voxel_end(void)
{


}


