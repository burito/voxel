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


extern int killme;		/* global killswitch */
extern int sys_width;	/* dimensions of default screen */
extern int sys_height;
extern int vid_width;	/* dimensions of our part of the screen */
extern int vid_height;
extern int mouse_x;		/* position */
extern int mouse_y;
extern int mickey_x;	/* velocity */
extern int mickey_y;
extern char mouse[3];	/* button status 0=up 1=down */
extern char keys[];


int main_init(int argc, char *argv[]);
void main_loop(void);
void main_end(void);

extern int fullscreen;
extern int fullscreen_toggle;

extern const int sys_ticksecond;	/* ticks in a second */
long long sys_time(void);

extern float time;


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void shell_browser(char *url);

#include "keyboard.h"
