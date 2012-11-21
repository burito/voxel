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
#include <math.h>

#include <GL/glew.h>

#include "3dmaths.h"
#include "main.h"
#include "voxel.h"

void free_vram(void)
{
	int mem = 0;
//	glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &mem);
	glGetIntegerv(0x9049, &mem);
	glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, &mem);
	glGetError();
	printf("Free vram: %d\n", mem);
}


int main_init(int argc, char *argv[])
{
	switch(argc) {
	case 1:
	case 2:
		if(strlen(argv[1]))
			voxel_init(argv[1]);
		else
			voxel_init("data/xyzrgb-dragon.oct");
		break;
	default:
		return 1;
	}

	printf("\nWelcome!\n");
	printf("\tleft mouse aims the camera\n");
	printf("\tright mouse moves forwards/back & left/right\n");
	printf("\tmiddle mouse moves up and down\n");
	printf("\tF11 toggles fullscreen\n");
	printf("\tS toggles swimming/flying\n");
	printf("\tP prints current coordinates\n");
	printf("\tEscape quits.\n");

	return 0;
}

// location of new visual artefact
/*
float3 pos = {2.315986, 0.594000, 0.159295};
float3 angle = {-0.248000, 2.299999, 0.000000};
*/
float3 pos = {0.824341, 0.433000, 0.207066};
float3 angle = {-0.106000, -0.166001, 0.000000};
int p_swim = 0;

void main_loop(void)
{
	float3 req = {0,0,0};
	float nice = 0.007;

	if(keys[KEY_ESCAPE])
	{
		killme=1;
	}
	if(keys[KEY_W])
	{
		req.z -= nice;
	}
	if(keys[KEY_S])
	{
		req.z += nice;
	}
	if(keys[KEY_A])
	{
		req.x += nice;
	}
	if(keys[KEY_D])
	{
		req.x -= nice;
	}
	if(keys[KEY_LCONTROL])
	{
		req.y -= nice;
	}
	if(keys[KEY_SPACE])
	{
		req.y += nice;
	}
		
//		pos = {0.824341, 0.433000, 0.207066};
//		angle = {-0.106000, -0.166001, 0.000000};
//		pos = {-0.009393, 0.450000, 0.049400};
//		angle = {-0.249000, -1.048000, 0.000000};


	if(mouse[0])
	{
		angle.x += mickey_y * 0.003;
		angle.y -= mickey_x * 0.003;
	}
	if(mouse[2]) // right
	{
		req.x += mickey_x *0.001;
		req.z -= mickey_y *0.001;
	}
	if(mouse[1]) // middle
	{
		pos.y -= mickey_y *0.001;
	}
	if(keys[KEY_O])
	{
		p_swim = !p_swim;
		keys[KEY_S] = 0;
		printf("Swimming is %s.\n", p_swim ? "engaged" : "off");
	}
	if(keys[KEY_P])
	{
		printf("float3 pos = {%f, %f, %f};\n", pos.x, pos.y, pos.z);
		printf("float3 angle = {%f, %f, %f};\n", angle.x, angle.y, angle.z);
	}

	if(p_swim)
	{
		float cx = cos(angle.x), sx = sin(angle.x), ty = req.y;
		req.y = req.y * cx - req.z * sx;	// around x
		req.z = ty * sx + req.z * cx;
	}


	float cy = cos(angle.y), sy = sin(angle.y), tx = req.x;
	req.x = req.x * cy + req.z * sy;	// around y
	req.z = tx * sy - req.z * cy;


	F3ADD(pos, pos, req);

	glClearColor(0.1,0.1,0.1,1);
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float ysize = (float)vid_height/(float)vid_width*0.5;
	glOrtho(0, 1.0, 0.5-ysize, 0.5+ysize, -1, 1000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	voxel_draw();
}


void main_end(void)
{
	voxel_end();
}


