/*
Copyright (c) 2013 Daniel Burke

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
#include <GL/glew.h>
#include <math.h>
#include "main.h"
#include "mesh.h"
#include "gui.h"
#include "ocl.h"

typedef struct LOADING
{
	char * filename;
	int passes;
	float *pass;
} LOADING;

const int max_loading = 6;
LOADING currently_loading[6];


void open_target(char * filename)
{
	

}


long long time_start = 0;
float time = 0;

OCLCONTEXT *ocl=NULL;

int main_init(int argc, char *argv[])
{
	GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat mat_shininess[] = { 50.0 };
	GLfloat light_position[] = { -100.0, -100.0, -100.0, 0.0 };
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
//	glEnable(GL_LIGHTING);

	if(ocl_init())
		printf("Initialising OpenCL failed.\n");

	time_start = sys_time();

	return gui_init(argc, argv);
}

float4 pos = { 0, 0, -2, 0};
float4 angle = {0,0,0,0};
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


	time = (float)(sys_time() - time_start)/(float)sys_ticksecond;

	gui_input();
	ocl_loop();
	gui_draw();
}



void main_end(void)
{
	ocl_end();
	gui_end();

}



