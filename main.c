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

#include "mesh.h"
#include "gui.h"


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



int main_init(int argc, char *argv[])
{
	return gui_init(argc, argv);

}

void main_loop(void)
{
	gui_loop();

}



void main_end(void)
{
	gui_end();

}



