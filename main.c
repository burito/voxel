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

#include <GL/glew.h>

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

	ocl = ocl_init();

	time_start = sys_time();

	return gui_init(argc, argv);
}

void main_loop(void)
{

	time = (float)(sys_time() - time_start)/(float)sys_ticksecond;

	ocl_loop(ocl);
	gui_loop();
}



void main_end(void)
{
	ocl_free(ocl);
	gui_end();

}



