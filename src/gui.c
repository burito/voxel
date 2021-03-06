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
#define CL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/glew.h>
#endif

#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>


#include "3dmaths.h"
#include "global.h"
#include "mesh_gl.h"
#include "gui.h"

#include "clvoxel.h"
#include "gpuinfo.h"



#include "fontstash.h"
#include "glfontstash.h"

static FONScontext* fs = NULL;

static int fontFixed = FONS_INVALID;
static int fontFixedBold = FONS_INVALID;
static int fontSans = FONS_INVALID;
static int fontSansBold = FONS_INVALID;



widget *widget_root = 0;
widget *latched = 0;

widget* widget_new(int2 pos, int2 size)
{
	widget *ret = 0;
	ret = malloc(sizeof(widget));
	if(ret == NULL)
	{
		log_fatal("malloc() failed\n");
		return 0;
	}

	memset(ret, 0, sizeof(widget));
	ret->pos = pos;
	ret->size = size;
	ret->fontsize = 20.0f;
	ret->fontface = fontSansBold;
	return ret;
}

void widget_free(widget* w)
{
	if(!w)return;
	if(latched == w)latched = widget_root;
	widget_free(w->child);
	widget_free(w->next);
	if(w->free)w->free(w);
	free(w);
}


widget* widget_shoot(widget *w, int2 pos)
{
	if(!w)return 0;
	if(w->noClick)return 0;
	if((pos.x > w->pos.x && pos.x < (w->pos.x + w->size.x)) &&
	   (pos.y > w->pos.y && pos.y < (w->pos.y + w->size.y)))
	{

		widget *ret = 0;
		int2 relative = { pos.x - w->pos.x, pos.y - w->pos.y };
		ret = widget_shoot(w->child, relative);
		if(ret)return ret;

		w->delta.x = mouse_x - pos.x;
		w->delta.y = mouse_y - pos.y;
		return w;
	}
	else return widget_shoot(w->next, pos);
}


void widget_item_draw(widget *w)
{
	if(!w)return;
	if(w->parent)
	{
		w->drawpos = add(w->pos, w->parent->drawpos);
	}
	else w->drawpos = w->pos;

	widget_item_draw(w->next);
	glTranslatef((GLfloat)w->pos.x, (GLfloat)-w->pos.y, 0.0f);
	if(w->draw)w->draw(w);
	widget_item_draw(w->child);
	glTranslatef((GLfloat)-w->pos.x, (GLfloat)w->pos.y, 0.0f);
}

void widget_add(widget *w)
{
	if(!w)return;
	if(!widget_root){ widget_root = w; return; }
	w->next = widget_root->next;
	widget_root->next = w;
	if(w->next)w->next->prev = w;
	w->prev = widget_root;
}

void widget_child_add(widget *w, widget *c)
{
	if(!w)return;
	if(!c)return;
	if(w->child)w->child->prev = c;
	c->next = w->child;
	c->parent = w;
	w->child = c;
}

void widget_remove(widget *w)
{
	if(!w)return;
	if(w->prev)
	{
		w->prev->next = w->next;
	}
	else if(w->parent)
	{
		w->parent->child = w->next;
	}
	else
	{
/* this is only a problem if called on a root widget,
 * which this func should not, it's designed to make menu's disappear */
	}

	if(w->next)
	{
		w->next->prev = w->prev;
	}
	w->prev = w->next = w->parent = 0;
}

void widget_text_draw(widget *w)
{
	if(!w)return;
	glColor4f(1,1,1,1);
	fonsSetSize(fs, w->fontsize);
	fonsSetFont(fs, w->fontface);
	fonsDrawText(fs, 10, 10, w->data, NULL);
}


void draw_rect(int w, int h)
{
	glBegin(GL_QUADS);
	glTexCoord2i(0,0);
	glVertex2i(0,0);

	glTexCoord2i(0,1);
	glVertex2i(0,-h);

	glTexCoord2i(1,1);
	glVertex2i(w,-h);

	glTexCoord2i(1,0);
	glVertex2i(w,0);
	glEnd();
}

void widget_destroy(widget *w)
{
	if(!w)return;
	if(w->prev)
	{
		w->prev->next = w->next;
	}
	else if(w->parent)
	{
		w->parent->child = w->next;
	}
	else
	{
		widget_root = w->next;
	}
	if(w->next)
	{
		w->next->prev = w->prev;
	}
	w->prev = w->next = w->parent = 0;
	widget_free(w);
}


void widget_rect_draw(widget *w)
{
	glColor4f(0.2, 0.2, 0.2, 1.0f);
	draw_rect(w->size.x, w->size.y);
}



void widget_button_draw(widget *w)
{
	float colour = 0.0f;
	if(w->clicked)colour = 0.3f;
	glColor4f(colour, colour, colour, 0.4f);
	draw_rect(w->size.x, w->size.y);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	fonsSetSize(fs, w->fontsize);
	fonsSetFont(fs, w->fontface);
	fonsDrawText(fs, 10, -(w->size.y/2+5), w->data, NULL);
}


void widget_button_onclick(widget *w)
{
	w->clicked = 1;
//	w->delta.x = mouse_x - w->pos.x;
//	w->delta.y = mouse_y - w->pos.y;
}


void widget_button_release(widget *w)
{
	int2 test;
	test.x = w->delta.x + w->pos.x;
	test.y = w->delta.y + w->pos.y;
	if(mouse_x > test.x)
	if(mouse_x < test.x + w->size.x)
	if(mouse_y > test.y)
	if(mouse_y < test.y + w->size.y)
	{
		if(w->action)w->action(w);
	}
}

void widget_button_free(widget *w)
{
	if(!w)return;
	if(w->data)free(w->data);
//	if(w->data2)free(w->data2);
}


widget* widget_button_new(int x, int y, const char* label)
{
	int2 p = {x, y}, s = {200 ,30};
	widget *w = widget_new(p, s);
	if(!w)
	{
		printf("widget_new() failed\n");
	}
	w->data = strdup(label);
	w->draw = widget_button_draw;
	w->onclick = widget_button_onclick;
	w->release = widget_button_release;
//	w->free = widget_button_free;
	return w;
}

widget* widget_text_new(int x, int y, char* label)
{
	int2 p = {x, y}, s = {0 ,30};
	widget *w = widget_new(p, s);
	w->data = label;
	w->draw = widget_text_draw;
	w->noClick = 1;
//	w->free = widget_button_free;
	return w;
}

void widget_window_draw(widget *w)
{
	GLfloat colour=0.4f;
	glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
	draw_rect(w->size.x, w->size.y);
	glColor4f(colour, colour, colour, 1.0f);
	glTranslatef(0, -5, 0);
	draw_rect(w->size.x, -3);
	glTranslatef(0, -25, 0);
	glColor4f(1,1,1,1);

	fonsSetSize(fs, 24.0f);
	fonsSetFont(fs, fontSans);
	fonsDrawText(fs, 20, 7, w->data, NULL);

	fonsSetSize(fs, 14.0f);
	fonsSetFont(fs, fontFixedBold);
	fonsDrawText(fs, w->size.x - 35, 10, "END", NULL);

	glColor4f(colour, colour, colour, 1.0f);
	draw_rect(w->size.x, -2);
	glTranslatef(0, 30, 0);

	glColor4f(0.5f, 0.5f, 0.2f, 0.4f);
	switch(w->clicked)
	{
	case 2:		// dragging
		glTranslatef(0, -5, 0);
		draw_rect(w->size.x, 23);
		glTranslatef(0, 5, 0);
		break;

	case 3:		// end button
		glTranslatef(w->size.x, -5, 0);
		draw_rect(-50, 23);
		glTranslatef(-w->size.x, 5, 0);
		break;

	case 4:		// resize bottom
		glTranslatef(0, -w->size.y, 0);
		draw_rect(w->size.x, -10);
		glTranslatef(0, w->size.y, 0);
		break;

	case 5:		// resize bottom left
		glTranslatef(0, -w->size.y, 0);
		draw_rect(10, -10);
		glTranslatef(0, w->size.y, 0);
		break;

	case 6:		// resize bottom right
		glTranslatef(w->size.x, -w->size.y, 0);
		draw_rect(-10, -10);
		glTranslatef(-w->size.x, w->size.y, 0);
		break;

	case 7:		// resize left
		draw_rect(10, w->size.y);
		break;

	case 8:		// resize right
		glTranslatef(w->size.x, 0, 0);
		draw_rect(-10, w->size.y);
		glTranslatef(-w->size.x, 0, 0);
		break;

	}

}

void widget_window_click(widget *w)
{
	switch(w->clicked)
	{
	case 2:		// dragging
		w->pos.x -= mickey_x;
		w->pos.y -= mickey_y;
		break;

	case 3:		// end
		break;

	case 4:		// resize down
		w->size.y -= mickey_y;
		break;

	case 5:		// resize bottom left
		w->pos.x -= mickey_x;
		w->size.x += mickey_x;
		w->size.y -= mickey_y;
		break;

	case 6:		// resize bottom right
		w->size.x -= mickey_x;
		w->size.y -= mickey_y;
		break;

	case 7:		// resize left
		w->pos.x -= mickey_x;
		w->size.x += mickey_x;
		break;

	case 8:		// resize right
		w->size.x -= mickey_x;
		break;

	case 9:		// rotating object
		if(!w->data3)break;
		vec3 *p = w->data3;
		p->y -= mickey_x;
		p->x -= mickey_y;
		break;

	}
}

void widget_window_onclick(widget *w)
{
	widget_remove(w);
	widget_add(w);		// send to foreground
	w->delta.x = mouse_x - w->pos.x;
	w->delta.y = mouse_y - w->pos.y;

	if(w->delta.y > 5)
	if(w->delta.y < 30)
	{
		if( w->delta.x > (w->size.x - 50) )
				w->clicked=3;	// end
		else
			w->clicked=2;		// dragging
		return;
	}

	if(w->noResize)return;

	if(w->delta.y > w->size.y - 10)	// dragging down
	{
		w->clicked = 4;		// resize bottom
		if(w->delta.x < 10)w->clicked = 5;	// resize bottom left
		if(w->delta.x > (w->size.x-10))w->clicked = 6;	// resize bottom right
		return;
	}

	if(w->delta.x < 10)
	{
		w->clicked = 7;		// resize left
	}
	if(w->delta.x > (w->size.x - 10))
	{
		w->clicked = 8;		// resize right
	}

}

void widget_open_click(widget *w)
{
	widget_window_click(w);
}

void widget_open_onclick(widget *w)
{
	widget_window_onclick(w);
}

void widget_window_release(widget *w)
{
	w->delta.x = mouse_x - w->pos.x;
	w->delta.y = mouse_y - w->pos.y;
	if(3 != w->clicked)return;
	if(w->delta.y > 5)
	if(w->delta.y < 30)
	if(w->delta.x > (w->size.x - 50) )
	if(w->delta.x < w->size.x )
	{
		// end button clicked
		widget_destroy(w);
	}

}


widget* widget_window_new(int x, int y, char* title)
{
	int2 p = {x, y}, s = {400, 300};
	widget *w = widget_new(p, s);
	w->draw = widget_window_draw;
	w->data = strdup(title);
	w->click = widget_window_click;
	w->onclick = widget_window_onclick;
	w->release = widget_window_release;
	w->free = widget_button_free;
	return w;
}

void widget_draw(widget *w)
{
	if(!w)return;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, vid_width, 0, vid_height, -1, 5000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glColor4ub(255,255,255,255);
	glTranslatef(0, vid_height, 0);
	widget_item_draw(w);
}

void widget_menu_draw(widget *w)
{
	float colour = 0.0f;
	if(w->clicked)colour = 0.3f;
	if(mouse_x > w->parent->pos.x)
	if(mouse_x < w->parent->pos.x + w->parent->size.x)
	if(mouse_y > w->parent->pos.y + w->pos.y)
	if(mouse_y < w->parent->pos.y + w->pos.y + w->size.y)
	{	// hovering
		if(!w->noClick)colour = 0.1;
	}
	glColor4f(colour, colour, colour, 0.4f);
	draw_rect(w->parent->size.x, w->size.y);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	fonsSetSize(fs, w->fontsize);
	fonsSetFont(fs, w->fontface);
	fonsDrawText(fs, 10, -(w->size.y/2+5), w->data, NULL);
}

void widget_menu_separator_draw(widget *w)
{
	if(!w)return;
	float colour = 0.0f;
	glColor4f(colour, colour, colour, 0.6f);
	draw_rect(w->parent->size.x, w->size.y);
}

void widget_menu_onclick(widget *w)
{
	widget_add((widget*)w->data2);
}

void widget_menu_release(widget *w)
{
	widget_remove((widget*)w->data2);
	widget *wdata = (widget*)w->data2;
	if(!wdata)return;
	widget *x = wdata->child;
	if(!x)return;
	while(x)
	{
		if(mouse_x > x->parent->pos.x)
		if(mouse_x < x->parent->pos.x + x->parent->size.x)
		if(mouse_y > x->parent->pos.y + x->pos.y)
		if(mouse_y < x->parent->pos.y + x->pos.y + x->size.y)
		{
			if(x->action)x->action(x);
			return;
		}
		x = x->next;
	}
}

/*
 * Widget List Functions
 */

void widget_list_draw(widget *w)
{
	if(!w)return;
	float height = 0.0f;
	float fascend=0, fdescend=0;
	fonsSetSize(fs, w->fontsize);
	fonsSetFont(fs, w->fontface);
	fonsVertMetrics(fs, &fascend, &fdescend, &height);
	float hoff = -fascend;
	float fullheight = (float)w->count * height;
	float totalheight = (fullheight - (float)w->size.y) / fullheight;
	int y = 0;
	char **names = (char**)w->data;
	widget_rect_draw(w);

	glEnable(GL_SCISSOR_TEST);
	glScissor(w->drawpos.x, vid_height - w->drawpos.y - w->size.y, w->size.x, w->size.y);


	glColor4f(1,1,1,1);
	if( fullheight < w->size.y )
	{	// scrollbar not needed
		for(int i=0; i<w->count; i++)
		{
			if(i == w->selected)
			{
				glTranslatef(0, y, 0);
				glColor4f(1,1,1,0.2);
				draw_rect(w->size.x, height);
				glTranslatef(0, -y, 0);
			}
			glColor4f(1,1,1,1);
			fonsDrawText(fs, 10, y+hoff, names[i], NULL);
			y -= height;
		}
	}
	else
	{
		float smooth = (float)w->count * (w->percent * totalheight);
		double mtmp = 0;
		int ysmooth = modf(smooth, &mtmp) * height;

		int i = smooth;
		for(; i<w->count; i++)
		{
			if(i == w->selected)
			{
				glTranslatef(0, y+ysmooth, 0);
				glColor4f(1,1,1,0.2);
				draw_rect(w->size.x-20, height);
				glTranslatef(0, -(y+ysmooth), 0);
			}
			glColor4f(1,1,1,1);
			fonsDrawText(fs, 10, y+hoff+ysmooth, names[i], NULL);
			y -= height;
			if( -y > w->size.y)
				break;
		}
		glColor4f(0,0,0,0.5);
		glTranslatef(w->size.x - 20, 0, 0);
		draw_rect(20, w->size.y);
		glColor4f(1,1,1,0.9);

		float scrollfactor = w->size.y / fullheight;
		float barheight = scrollfactor * w->size.y;

		glTranslatef(0, -w->percent*(float)(w->size.y-barheight), 0);
		draw_rect(20, barheight);
		glTranslatef(0, w->percent*(float)(w->size.y-barheight), 0);

		glTranslatef(-w->size.x + 20, 0, 0);
	}
	glDisable(GL_SCISSOR_TEST);
}

void widget_list_click(widget *w)
{
	if(!w)return;
	float height = 0.0f;
	fonsSetSize(fs, w->fontsize);
	fonsSetFont(fs, w->fontface);
	fonsVertMetrics(fs, NULL, NULL, &height);
	int2 click;
	click.x = mouse_x - w->delta.x - w->pos.x;
	click.y = mouse_y - w->delta.y - w->pos.y;

	float fullheight = (float)w->count * height;
	float totalheight = (fullheight - (float)w->size.y) / fullheight;
//	if(click.x < 0)return;
//	if(click.x > w->size.x)return;
//	if(click.y < 0)click.y = 0;
//	if(click.y > w->size.y)click.y = w->size.y;

	if(2 == w->clicked) // scrollbar!
	{

		float scrollfactor = w->size.y / fullheight;
		float barheight = scrollfactor * w->size.y;

		float offset = w->percent;
		offset -= (float)mickey_y / (float)(w->size.y-barheight);
		if(offset < 0.0) offset = 0.0;
		if(offset > 1.0) offset = 1.0;
		w->percent = offset;
	}
	else
	{
		int i = (float)w->count * (w->percent * totalheight);
		int offset = (float)click.y / height;
		offset += i;
		if(offset > w->count)return;
		w->selected = offset;
//		char **file = w->data;
//		printf("this one %s\n", file[offset]);

	}
}

void widget_list_onclick(widget *w)
{
	if(!w)return;
	float height = 0.0f;
	fonsSetSize(fs, w->fontsize);
	fonsSetFont(fs, w->fontface);
	fonsVertMetrics(fs, NULL, NULL, &height);
	int2 click;
	click.x = mouse_x - w->delta.x - w->pos.x;
	click.y = mouse_y - w->delta.y - w->pos.y;
	float fullheight = (float)w->count * height;
	float totalheight = (fullheight - (float)w->size.y) / fullheight;

	if(click.x < 0)return;
	if(click.x > w->size.x)return;
	if(click.y < 0)click.y = 0;
	if(click.y > w->size.y)click.y = w->size.y;

	if(click.x > (w->size.x - 20))
	{
		w->clicked = 2; // scrollbar!
	}
	else
	{
		w->clicked = 1; // selection
		int i = (float)w->count * (w->percent * totalheight);
		int offset = (float)click.y / height;
		offset += i;
		if(offset > w->count)return;
		w->selected = offset;

	}
}

void widget_list_free(widget *w)
{
	char **list = w->data;
	for(int i=0; i<w->count; i++)
		free(list[i]);
	free(w->data);
}



widget* widget_list_new(int x, int y, char **list, int count)
{
	int2 p = {x,y}, s = {150, 150};
	widget *w = widget_new(p, s);
	w->draw = widget_list_draw;
	w->click = widget_list_click;
	w->onclick = widget_list_onclick;
	w->data = list;
	w->free = widget_list_free;
	w->count = count;
	w->selected = -1;
	return w;
}


void widget_window_obj_draw(widget *w)
{
	widget_window_draw(w);
	struct MESH_OPENGL *m = w->data2;

	glPushMatrix();
	glClearDepth(5000.0f);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glColor4f(1,1,1,1);
	glTranslatef(10, -w->size.y+10, -2000);

	int scale = w->size.y - 50;

	if((w->size.x-20) > scale)
	{
		float delta = (float)(w->size.x - 20 - scale) * 0.5f;
		glTranslatef(delta, 0, 0);
	}
	else
	{
		scale = w->size.x-20;
		float delta = (float)(w->size.y - 50 - scale) * 0.5f;
		glTranslatef(0, delta, 0);
	}

	glScalef(scale, scale, scale);

	vec3 *p = w->data3;
	glTranslatef(0.5,0.5, 0.5f);
	glRotatef(p->x, 1.0f, 0.0f, 0.0f);
	glRotatef(p->y, 0.0f, 1.0f, 0.0f);
	glRotatef(p->z, 0.0f, 0.0f, 1.0f);
	glTranslatef(-0.5,-0.5, -0.5f);

	glEnable(GL_LIGHTING);
	glEnable(GL_NORMALIZE);
	mesh_draw(m);
	glDisable(GL_NORMALIZE);
	glDisable(GL_LIGHTING);
	glPopMatrix();
	glDisable(GL_DEPTH_TEST);
}

void widget_window_obj_free(widget *w)
{
	struct MESH_OPENGL *m = w->data2;
	mesh_free(m);
}

void widget_window_obj_onclick(widget *w)
{
	if(!w)return;
	widget_window_onclick(w);
	if(!w->clicked)
		w->clicked = 9;		// rotating object
}

widget* spawn_obj(char* filename)
{
	widget *w = widget_window_new(100, 100, filename);
	struct MESH_OPENGL *m = mesh_load(filename);
	w->data2 = m;
	w->draw = widget_window_obj_draw;
	w->free = widget_window_obj_free;
	w->onclick = widget_window_obj_onclick;
	vec3 *p = malloc(sizeof(vec3));
	p->x = p->y = p->z = 0;
	w->data3 = p;
	widget_add(w);
	return w;
}

widget* spawn_voxobj(char* filename)
{
//	widget *w = widget_window_new(100, 100, filename);
	struct MESH_OPENGL *m = mesh_load(filename);
	extern struct MESH_OPENGL *vobj;
	if(vobj)mesh_free(vobj);
	vobj = m;
	voxel_open();

//	w->data2 = m;
//	w->draw = widget_window_obj_draw;
//	w->free = widget_window_obj_free;
//	w->onclick = widget_window_obj_onclick;
//	vec3 *p = malloc(sizeof(vec3));
//	p->x = p->y = p->z = 0;
//	w->data3 = p;
//	widget_add(w);
	return NULL;
}


/*
 * Open File Dialog
 */

static int cmp_str(const void *p1, const void *p2)
{
	return strcmp(* (char * const *) p1, * (char * const *) p2);
}

void open_test(widget *w)
{
	if(!w)return;
	if(!w->parent)return;
	widget *p = w->parent;
	if(!p->child)return;
	widget *f = p->child;		// file dialog
	if(-1 == f->selected)return;			// is a file selected?
	if(!f->data)return;
	char **names = (char**)f->data;
	char *path = (char*)p->data2;
	char buf[1000];
	sprintf(buf, "%s/%s", path, names[f->selected]);
	int len = strlen(buf);
	switch(buf[len-3]) {
	case 'o':
	case 'O':
		switch(buf[len-2]) {
		case 'b':
		case 'B':	// obj - Wavefront OBJ
			spawn_obj(buf);
			widget_destroy(p);
			break;
		}

	default:
		break;
	}
}

void open_voxel(widget *w)
{
	if(!w)return;
	if(!w->parent)return;
	widget *p = w->parent;
	if(!p->child)return;
	widget *f = p->child;		// file dialog
	if(-1 == f->selected)return;			// is a file selected?
	if(!f->data)return;
	char **names = (char**)f->data;
	char *path = (char*)p->data2;
	char buf[1000];
	sprintf(buf, "%s/%s", path, names[f->selected]);
	int len = strlen(buf);
	switch(buf[len-3]) {
	case 'o':
	case 'O':
		switch(buf[len-2]) {
		case 'b':
		case 'B':	// obj - Wavefront OBJ
			spawn_voxobj(buf);
			widget_destroy(p);
			break;
		}
		break;

	default:
		break;
	}
}

void spawn_open(widget *x)
{
	widget *w = widget_window_new(100, 100, "OPEN...");
	w->onclick = widget_open_onclick;
	w->click = widget_open_click;
	widget *b = widget_button_new(10, 40, "Open");
	b->action = x ? open_test : open_voxel;

	widget_child_add(w, b);


	int dmax = 100, fmax = 100;
	int dcnt = 0, fcnt = 0;
	char **dirs;
	char **files;

	dirs = malloc(sizeof(char*)*dmax);
	files = malloc(sizeof(char*)*fmax);

	char *path = "./data";
	w->data2 = path;
	DIR *dir = opendir(path);
	char tmp[1000];
	struct dirent *ent;
	while((ent = readdir(dir)))
	{
		if(ent->d_name[0] == '.')continue;

		struct stat s;
		memset(&s, 0, sizeof(struct stat));
		memset(tmp, 0, 1000);
		sprintf(tmp, "%s/%s", path, ent->d_name);
		stat(tmp, &s);
		if( S_ISDIR(s.st_mode) )
		{
			if(dcnt == dmax)
			{
				printf("dont forget to malloc some more\n");
				return;
			}
			dirs[dcnt++] = strdup(ent->d_name);
		} else
		if( S_ISREG(s.st_mode) )
		{
			if(fcnt == fmax)
			{
				printf("dont forget to malloc some more\n");
				return;
			}
			if(x)
			{
				if(strstr(ent->d_name, ".obj"))
					files[fcnt++] = strdup(ent->d_name);
			}
			else
			{
				if(strstr(ent->d_name, ".obj"))
					files[fcnt++] = strdup(ent->d_name);
			}
		}
		else printf("Unexpected Filetype= %d \"%s\"\n", s.st_mode, ent->d_name);

	}
	closedir(dir);

	qsort(dirs, dcnt, sizeof(char*), cmp_str);
	qsort(files, fcnt, sizeof(char*), cmp_str);

	widget *item = widget_list_new(10, 80, dirs, dcnt);
	item->size.x = w->size.x / 3 - 15;
	item->size.y = w->size.y - 100;
	widget_child_add(w, item);
	item = widget_list_new(w->size.x/3 + 5, 80, files, fcnt);
	item->size.x = w->size.x / 3 * 2 - 15;
	item->size.y = w->size.y - 100;
	widget_child_add(w, item);
	item->action = open_test;
	widget_add(w);
}

void spawn_voxopen(widget *x)
{
	spawn_open(NULL);
}


/*
 * Widget Menu functions
 */

widget* widget_menu_new(void)
{
	int2 p = {0,0}, s = {200, 30};
	widget *w = widget_new(p, s);
	w->draw = widget_rect_draw;
	return w;
}

widget* widget_menu_add(widget *w, const char* label)
{
	if(!w)return 0;
	widget *ret = widget_button_new(0, 0, label);

	if(!ret)
	{
		log_fatal("widget_button_new()");
	}
	ret->onclick = widget_menu_onclick;
	ret->release = widget_menu_release;
	ret->parent = w;
	if(w->child)
	{
		ret->pos.x = w->child->pos.x + w->child->size.x;
		ret->next = w->child;
		w->child->prev = ret;
	}
	w->child = ret;

	fonsSetSize(fs, w->fontsize);
	fonsSetFont(fs, w->fontface);

	float width;
	width = fonsTextBounds(fs, 0, 0, label, NULL, NULL);
	ret->size.x = 20+ width;

	return ret;
}

widget* widget_menu_item_add(widget *w, char* label, void (*action)(widget*))
{
	if(!w)return 0;
	widget *wdata = (widget*)w->data2;
	if(!wdata)
	{
		wdata = widget_menu_new();
		w->data2 = wdata;
		wdata->draw = 0;
		wdata->size.x = w->size.x;
		wdata->pos.x = w->pos.x;
		wdata->pos.y = w->pos.y + w->size.y;
	}
	widget *ret = widget_button_new(0, 0, label);
	ret->draw = widget_menu_draw;
	ret->action = action;
	ret->parent = wdata;
	if(wdata->child)
	{
		wdata->child->prev = ret;
		ret->pos.y = wdata->child->pos.y + wdata->child->size.y;
	}
	ret->next = wdata->child;
	wdata->child = ret;

	fonsSetSize(fs, ret->fontsize);
	fonsSetFont(fs, ret->fontface);
	float width;
	width = fonsTextBounds(fs, 0, 0, label, NULL, NULL);
	ret->size.x = 20+ width;
	if(wdata->size.x < ret->size.x)wdata->size.x = ret->size.x;

	return ret;
}

/*
 * About Box
 */

void widget_url_action(widget *w)
{
	sys_browser(w->data2);
}

widget* widget_url_new(int x, int y, char *label, char *url)
{
	widget *w = widget_button_new(x, y, label);
	w->data2 = url;
	w->action = widget_url_action;
	return w;
}

void spawn_gpuinfo(widget *x)
{
	widget *item, *w = widget_window_new(100, 100, "GPU Information");
	item = widget_text_new(10, 65, "OpenGL");
	item->fontsize = 30.0f;
	item->fontface = fontFixedBold;
	widget_child_add(w, item);
	item = widget_text_new(20, 90, strdup(
		(const char*)glGetString(GL_VENDOR) ));
	widget_child_add(w, item);
	item = widget_text_new(20, 110, strdup(
		(const char*)glGetString(GL_RENDERER) ));
	widget_child_add(w, item);
	item = widget_text_new(20, 130, strdup(
		(const char*)glGetString(GL_VERSION) ));
	widget_child_add(w, item);
	item = widget_text_new(20, 150, strdup(
		(const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) ));
	widget_child_add(w, item);

	w->size.x = 330;
	w->size.y = 160;
	w->noResize = 1;
	widget_add(w);
}

void spawn_http_auth(widget *x)
{
	widget *item, *w = widget_window_new(100, 100, "Authorised Devices");
	item = widget_text_new(10, 65, "OpenGL");
	item->fontsize = 30.0f;
	item->fontface = fontFixedBold;
	widget_child_add(w, item);
	item = widget_text_new(20, 90, strdup(
		(const char*)glGetString(GL_VENDOR) ));
	widget_child_add(w, item);
	item = widget_text_new(20, 110, strdup(
		(const char*)glGetString(GL_RENDERER) ));
	widget_child_add(w, item);
	item = widget_text_new(20, 130, strdup(
		(const char*)glGetString(GL_VERSION) ));
	widget_child_add(w, item);
	item = widget_text_new(20, 150, strdup(
		(const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) ));
	widget_child_add(w, item);

	w->size.x = 330;
	w->size.y = 155;
	w->noResize = 1;
	widget_add(w);
}

void spawn_http_pend(widget *x)
{
	widget *item, *w = widget_window_new(100, 100, "Pending Devices");
	item = widget_text_new(10, 65, "OpenGL");
	item->fontsize = 30.0f;
	item->fontface = fontFixedBold;
	widget_child_add(w, item);
	item = widget_text_new(20, 90, strdup(
		(const char*)glGetString(GL_VENDOR) ));
	widget_child_add(w, item);
	item = widget_text_new(20, 110, strdup(
		(const char*)glGetString(GL_RENDERER) ));
	widget_child_add(w, item);
	item = widget_text_new(20, 130, strdup(
		(const char*)glGetString(GL_VERSION) ));
	widget_child_add(w, item);
	item = widget_text_new(20, 150, strdup(
		(const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) ));
	widget_child_add(w, item);

	w->size.x = 330;
	w->size.y = 155;
	w->noResize = 1;
	widget_add(w);
}
void spawn_about(widget *x)
{
	widget *w = widget_window_new(100, 100, "ABOUT");
	widget *item = widget_text_new(20, 110, "\u00a9 2013 Daniel Burke");
	widget_child_add(w, item);
	item = widget_text_new(20, 80, "VOXEL TEST");
	item->fontsize = 40.0f;
	item->fontface = fontFixedBold;
	widget_child_add(w, item);
	item = widget_url_new(20, 120, "WWW",
			"http://danpburke.blogspot.com.au");
	item->size.x = 90;
	widget_child_add(w, item);
	item = widget_url_new(120, 120, "GitHub",
			"https://github.com/burito/voxel");
	item->size.x = 90;
	widget_child_add(w, item);
	item = widget_url_new(220, 120, "YouTube",
			"http://www.youtube.com/user/bur1t0/videos");
	item->size.x = 90;
	widget_child_add(w, item);
	w->size.x = 330;
	w->size.y = 170;
	w->noResize = 1;
	widget_add(w);
}

void spawn_credits(widget *x)
{
	widget *item, *w = widget_window_new(100, 100, "CREDITS");
	item = widget_text_new(20, 80, "3RD PARTY");
	item->fontsize = 40.0f;
	item->fontface = fontFixedBold;
	widget_child_add(w, item);
	item = widget_text_new(30, 120, "LIBRARIES");
	item->fontface = fontFixedBold;
	widget_child_add(w, item);
	item = widget_text_new(190, 120, "FONTS");
	item->fontface = fontFixedBold;
	widget_child_add(w, item);
	item = widget_url_new(20, 120, "GLEW",
			"http://glew.sourceforge.net");
	item->size.x = 140;
	widget_child_add(w, item);
	item = widget_url_new(20, 160, "Font Stash",
			"https://github.com/akrinke/Font-Stash");
	item->size.x = 140;
	widget_child_add(w, item);
	item = widget_url_new(20, 200, "stb_truetype.h",
			"http://nothings.org/");
	item->size.x = 140;
	widget_child_add(w, item);
	item = widget_url_new(20, 240, "stb_image.c",
			"http://nothings.org/");
	item->size.x = 140;
	widget_child_add(w, item);
	item = widget_url_new(20, 280, "zlib",
			"http://zlib.net/");
	item->size.x = 140;
	widget_child_add(w, item);
	item = widget_url_new(180, 120, "SourceSansPro",
			"https://github.com/adobe-fonts/source-sans-pro");
	item->size.x = 140;
	widget_child_add(w, item);
	item = widget_url_new(180, 160, "SourceCodePro",
			"https://github.com/adobe-fonts/source-code-pro");
	item->fontface = fontFixedBold;
	item->size.x = 140;
	widget_child_add(w, item);
//	item = widget_url_new(180, 200, "",
//			"");
//	item->size.x = 140;
//	widget_child_add(w, item);
	w->size.x = 340;
	w->size.y = 330;
	w->noResize = 1;
	widget_add(w);
}


/*
 * License Dialog
 */

void spawn_license(widget *x)
{
	widget *w = widget_window_new(100, 100, "LICENSE");
	widget *item;
	item = widget_text_new(20, 70, "Copyright (c) 2012 Daniel Burke");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 95, "This software is provided 'as-is', without any express or implied");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 110, "warranty. In no event will the authors be held liable for any damages");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 125, "arising from the use of this software.");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 155, "Permission is granted to anyone to use this software for any purpose,");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 170, "including commercial applications, and to alter it and redistribute it");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 185, "freely, subject to the following restrictions:");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 210, "	1. The origin of this software must not be misrepresented; you must not");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 225, "	claim that you wrote the original software. If you use this software");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 240, "	in a product, an acknowledgment in the product documentation would be");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 255, "	appreciated but is not required.");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 280, "	2. Altered source versions must be plainly marked as such, and must not be");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 295, "	misrepresented as being the original software.");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 320, "	3. This notice may not be removed or altered from any source");
	item->fontface = fontFixedBold;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 335, "	distribution.");
	item->fontface = fontFixed;	item->fontsize = 14.0f; widget_child_add(w, item);
	w->size.x = 570;
	w->size.y = 360;
	w->pos.x = 20;
	w->pos.y = 80;
	w->noResize = 1;
	widget_add(w);
}

void widget_menu_bool_draw(widget *w)
{
	float colour = 0.0f;
	if(w->clicked)colour = 0.3f;
	if(mouse_x > w->parent->pos.x)
	if(mouse_x < w->parent->pos.x + w->parent->size.x)
	if(mouse_y > w->parent->pos.y + w->pos.y)
	if(mouse_y < w->parent->pos.y + w->pos.y + w->size.y)
	{	// hovering
		if(!w->noClick)colour = 0.1;
	}
	glColor4f(colour, colour, colour, 0.4f);
	draw_rect(w->parent->size.x, w->size.y);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	fonsSetSize(fs, w->fontsize);
	fonsSetFont(fs, w->fontface);
	fonsDrawText(fs, 10, -(w->size.y/2+5), (*(int*)w->data2 ? "\u2611":"\u2610"), NULL);
	fonsDrawText(fs, 10, -(w->size.y/2+5), w->data, NULL);
}


void menu_killme(widget *w)
{
	killme = 1;
}


void menu_fullscreen(widget *w)
{
	fullscreen_toggle++;
}


void menu_texdraw(widget *w)
{
//	extern int texdraw;
//	texdraw = !texdraw;
}


widget* widget_menu_separator_add(widget *item)
{
	widget *w = widget_menu_item_add(item, "", 0);
	w->draw = widget_menu_separator_draw;
	w->size.y = 3;
	w->noClick = 1;
	return w;
}


int frame_number = 0;
#define MAX_FRAMES 60
long frame_time[MAX_FRAMES];
long long last_time;
int gui_init(int argc, char *argv[])
{
	memset(frame_time, 0, sizeof(long)*MAX_FRAMES);
	fs = glfonsCreate(512, 512, FONS_ZERO_BOTTOMLEFT);
	if (!fs)
	{
		printf("Could not create stash.\n");
		return -1;
	}

	fontFixed	= fonsAddFont(fs, "fixed",	"data/gui/SourceCodePro-Regular.ttf");
	if(FONS_INVALID == fontFixed)
	{
		printf("Failed to load Fixed Font\n");
		return 1;
	}
	fontFixedBold	= fonsAddFont(fs, "fixed-bold",	"data/gui/SourceCodePro-Regular.ttf");
	if(FONS_INVALID == fontFixedBold)
	{
		printf("Failed to load Fixed Bold Font\n");
		return 1;
	}
	fontSans	= fonsAddFont(fs, "sans",	"data/gui/SourceSansPro-Regular.ttf");
	if(FONS_INVALID == fontSans)
	{
		printf("Failed to load Sans Font\n");
		return 1;
	}
	fontSansBold	= fonsAddFont(fs, "sans-bold",	"data/gui/SourceSansPro-Regular.ttf");
	if(FONS_INVALID == fontSansBold)
	{
		printf("Failed to load Sans Bold Font\n");
		return 1;
	}

	widget *w;
	widget *menu = widget_menu_new();
	widget *item = widget_menu_add(menu, "File");
	widget_menu_item_add(item, "Open", spawn_open);
	widget_menu_item_add(item, "Voxel Open", spawn_voxopen);
	widget_menu_separator_add(item);
	widget_menu_item_add(item, "Exit", menu_killme);

	item = widget_menu_add(menu, "Display");
	w = widget_menu_item_add(item, "     Fullscreen - F11", menu_fullscreen);
	w->draw = widget_menu_bool_draw;
	w->data2 = &fullscreen;

	widget_menu_item_add(item, "GPU Information", spawn_gpuinfo);
	widget_menu_separator_add(item);
	widget_menu_item_add(item, "Rebuild Shaders", voxel_rebuildshader);

	item = widget_menu_add(menu, "Help");
	widget_menu_item_add(item, "Overview \u2560", 0);
	widget_menu_item_add(item, "Wiki \u26a0", 0);
	widget_menu_separator_add(item);
	widget_menu_item_add(item, "Credits \u2623", spawn_credits);
	widget_menu_item_add(item, "License", spawn_license);
	widget_menu_separator_add(item);
	widget_menu_item_add(item, "About", spawn_about);

	widget_add(menu);

	last_time = sys_time();
	return 0;
}


void gui_input(void)
{

	if(latched)
	{
		if(mouse[0])
		{
			if(latched->click)
			latched->click(latched);
		}
		else
		{
			if(latched->release)latched->release(latched);
			latched->clicked = 0;
			latched = 0;
		}
	}
	else if(mouse[0])
	{
		int2 mouse = { mouse_x, mouse_y };
		latched = widget_shoot(widget_root, mouse);
		if(latched)
		if(latched->onclick)latched->onclick(latched);
	}
}


void gui_draw(void)
{
	fonsClearState(fs);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	widget_draw(widget_root);

	long long delta_time, this_time = sys_time();
	delta_time = this_time - last_time;
	last_time = this_time;
//	double DeltaTime = (double)delta_time / (double)sys_ticksecond;

	frame_number++;
	if(frame_number>=MAX_FRAMES) frame_number=0;
	frame_time[frame_number] = (long)delta_time;
	long fps_sum = 0;
	for(int i=0; i<MAX_FRAMES; i++)
		fps_sum += frame_time[i];

	double fps = (double)MAX_FRAMES / ((double)fps_sum / (double)sys_ticksecond);

	glColor4f(1,1,1,1);
	char tempstr[50];
	sprintf(tempstr, "%4.1fHz", fps);

	fonsSetSize(fs, 30.0f);
	fonsSetFont(fs, fontSansBold);
	fonsDrawText(fs, vid_width-90, -26, tempstr, NULL);

#ifndef __APPLE__
	int i;
	for(i=0; i< nvml_device_count; i++)
	{
		sprintf(tempstr, "%d°C", nvml_gputemp[i]);
		fonsDrawText(fs, vid_width-70, -(50+i*50), tempstr, NULL);
	}
	for(int j=0; j< adl_device_count; j++)
	{
		sprintf(tempstr, "%d°C", adl_gputemp[j]);
		fonsDrawText(fs, vid_width-70, -(50+(i+j)*50), tempstr, NULL);
	}
#endif

}




void gui_end(void)
{
	if(fs)glfonsDelete(fs);
}
