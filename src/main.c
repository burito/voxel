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
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/glew.h>
#endif

#include <stdio.h>
#include <math.h>

#include "ocl.h"

#include "main.h"
#include "mesh.h"
#include "gui.h"
#include "clvoxel.h"
#include "gpuinfo.h"
#include "http.h"
#include "shader.h"


#include "log.h"

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
GLSLSHADER *shader_default=NULL;

extern const char git_version[];

int main_init(int argc, char *argv[])
{
	log_init();
	log_info("Version    : %s", git_version);
	log_info("GL Vendor  : %s", glGetString(GL_VENDOR) );
	log_info("GL Renderer: %s", glGetString(GL_RENDERER) );
	log_info("GL Driver  : %s", glGetString(GL_VERSION) );
	log_info("SL Version : %s", glGetString(GL_SHADING_LANGUAGE_VERSION) );

	int glver[2];
	glGetIntegerv(GL_MAJOR_VERSION, &glver[0]);
	glGetIntegerv(GL_MINOR_VERSION, &glver[1]);
	log_info("GL Version : %d.%d", glver[0], glver[1]);
	
#ifndef __APPLE__
	if(!GLEW_VERSION_4_3)
	{
		log_fatal("OpenGL 4.3 Required");
		return 1;
	}
	gpuinfo_init();
#endif

	shader_default = shader_load("data/shaders/default.vert",
			"data/shaders/default.frag");

	ocl_init();

	GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat mat_shininess[] = { 50.0 };
	GLfloat light_position[] = { -100.0, -100.0, -100.0, 0.0 };
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	time_start = sys_time();
	http_init();
	voxel_init();

	return gui_init(argc, argv);
}

int p_swim = 0;

// last digit of angle is x-fov, in radians
vec4 pos = {0.619069, 0.644001, 1.457747, 0.0};
vec4 angle = {-0.126000, -3.261001, 0.000000, M_PI*0.5};


void main_loop(void)
{
#ifndef __APPLE__
	gpuinfo_tick();
#endif
	for(int i=0; i<4; i++)
	{
		joy[i].fflarge = joy[i].lt;
		joy[i].ffsmall = joy[i].rt;
	}

	pos.w = 0.5 / (float)vid_width;

	vect req = {0,0,0};
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
		req.x -= nice;
	}
	if(keys[KEY_D])
	{
		req.x += nice;
	}
	if(keys[KEY_LCONTROL])
	{
		req.y -= nice;
	}
	if(keys[KEY_SPACE])
	{
		req.y += nice;
	}

	if(mouse[2]) /// right mouse
	{
		angle.x += mickey_y * 0.003;
		angle.y += mickey_x * 0.003;
	}
	if(keys[KEY_O])
	{
		p_swim = !p_swim;
		keys[KEY_S] = 0;
		log_verbose("Swimming is %s.", p_swim ? "engaged" : "off");
	}
	if(keys[KEY_P])
	{
		log_verbose("float4 pos = {%f, %f, %f, 0.0};", pos.x, pos.y, pos.z);
		log_verbose("float4 angle = {%f, %f, %f, M_PI*0.5};", angle.x, angle.y, angle.z);
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


	pos.xyz = add(pos.xyz, req);


	time = (float)(sys_time() - time_start)/(float)sys_ticksecond;

	glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glUseProgram(shader_default->prog);

	gui_input();
	voxel_loop();
	ocl_loop();
	gui_draw();
	http_loop();
}

void main_end(void)
{
#ifndef __APPLE__
	gpuinfo_end();
#endif
	voxel_end();
	ocl_end();
	gui_end();
	http_end();
}

