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
#include <stdlib.h>
#include <string.h>

#include <GL/glew.h>

#include "3dmaths.h"
#include "image.h"
#include "main.h"

#include "fontstash.h"

struct sth_stash* stash = 0;

typedef struct widget
{
	int2 pos, size, delta;
	struct widget *parent, *child, *next, *prev;
	int clicked;
	int noClick;
	void (*click)(struct widget*);
	void (*release)(struct widget*);
	void (*onclick)(struct widget*);
	void (*draw)(struct widget*);
	void (*action)(int arg);
	int action_arg;
	void *data;
} widget;

widget *widget_root = 0;
widget *latched = 0;

widget* widget_new(int2 pos, int2 size)
{
	widget *ret = 0;
	ret = malloc(sizeof(widget));
	if(!ret)
	{
		printf("malloc fail\n");
		return 0;
	}

	memset(ret, 0, sizeof(widget));
	ret->pos = pos;
	ret->size = size;
	return ret;
}

void widget_kill(widget* w)
{
	if(!w)return;
	widget_kill(w->child);
	widget_kill(w->next);
//	if(w->data)free(w->data);
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

void widget_draw_text(widget *w)
{
	if(!w)return;
	float dx = 0.0f;
	sth_begin_draw(stash);
	sth_draw_text(stash, 0,24.0f, 0, 0, w->data, &dx);
	sth_end_draw(stash);
//	sth_vmetrics(stash, 1,24, NULL,NULL,&height);
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
	if(w->prev)
	{	// then there is no parent
		w->prev->next = w->next;
		if(w->next)w->next->prev = w->prev;
	}
	if(w->parent)
	{	// then there is no prev
		w->parent->child = w->next;
		if(w->next)
		{
			w->next->prev = w->prev;
			w->next->parent = w->parent;
		}
	}
	w->next = 0;
	widget_kill(w);
}


void widget_rect_draw(widget *w)
{
	glColor4f(0.2, 0.2, 0.2, 1.0f);
	draw_rect(w->size.x, w->size.y);
}

void widget_menu_draw(widget *w)
{
	glColor4f(0.0, 0.0, 0.0, 0.5f);
	draw_rect(w->size.x, w->size.y);
}


void widget_button_draw(widget *w)
{
	float colour = 0.0f;
	if(w->clicked)colour = 0.3f;
	glColor4f(colour, colour, colour, 0.4f);
	draw_rect(w->size.x, w->size.y);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	sth_begin_draw(stash);
	sth_draw_text(stash, 2,20.0f, 10, -(w->size.y/2+5), w->data, 0);
	sth_end_draw(stash);
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
		if(w->action)w->action(w->action_arg);
	}

}


void widget_filemenu(void)
{
	widget *w;

	int2 p = {0, 30}, s = { 100, 100 };
	w = widget_new(p, s);
	w->draw = widget_menu_draw;
	w->release = widget_destroy;
	widget_add(w);
//	latched = w;
		
}


void widget_button_action(int arg)
{
	switch(arg) {
		case 0:
			printf("Default action\n");
			break;

		case 5:
			widget_filemenu();
			break;

		default:
			printf("Action %d initiated.\n", arg);
	}

}


widget* widget_button_new(int x, int y, char* label)
{
	int2 p = {x, y}, s = {200 ,30};
	widget *w = widget_new(p, s);
	w->data = label;
	w->draw = widget_button_draw;
	w->onclick = widget_button_onclick;
	w->release = widget_button_release;
	w->action = widget_button_action;
	return w;
}


void widget_draw_window(widget *w)
{
	GLfloat colour=0.4f;
	glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
	draw_rect(w->size.x, w->size.y);
	glColor4f(colour, colour, colour, 1.0f);
	glTranslatef(0, -5, 0);
	draw_rect(w->size.x, -3);

//	glTranslatef(0, -30, 0);

	if(3 == w->clicked)
	{
		glColor4f(0.5f, 0.5f, 0.2f, 0.4f);
		glTranslatef(w->size.x, 0, 0);
		draw_rect(-50, 23);
		glTranslatef(-w->size.x, 0, 0);

	}
	glTranslatef(0, -25, 0);

	glColor4f(1,1,1,1);
	sth_begin_draw(stash);
	sth_draw_text(stash, 1,24.0f, 20, 7, w->data, 0);
	sth_draw_text(stash, 0,14.0f, w->size.x - 35, 10, "END", 0);
	sth_end_draw(stash);
//	glTranslatef(0, 0, 0);
	glColor4f(colour, colour, colour, 1.0f);
	draw_rect(w->size.x, -2);
	glTranslatef(0, 30, 0);

}

void widget_window_click(widget *w)
{
	if(2 == w->clicked)		// dragging
	{
		w->pos.x -= mickey_x;
		w->pos.y -= mickey_y;
	}

}

void widget_window_onclick(widget *w)
{
	w->delta.x = mouse_x - w->pos.x;
	w->delta.y = mouse_y - w->pos.y;

	if(w->delta.y > 5)
	if(w->delta.y < 30)
	{
		if( w->delta.x > (w->size.x - 50) )
				w->clicked=3;	// end
		else
			w->clicked=2;		// dragging
	}
}

void widget_window_release(widget *w)
{
	if(3 != w->clicked)return;
	w->delta.x = mouse_x - w->pos.x;
	w->delta.y = mouse_y - w->pos.y;
	if(w->delta.y > 5)
	if(w->delta.y < 30)
	if(w->delta.x > (w->size.x - 50) )
	if(w->delta.x < w->size.x )
	{
		// end button clicked
	}

}


widget* widget_window_new(int x, int y, char* title)
{
	int2 p = {x, y}, s = {400, 300};
	widget *w = widget_new(p, s);
	w->draw = widget_draw_window;
	w->data = title;
	w->click = widget_window_click;
	w->onclick = widget_window_onclick;
	w->release = widget_window_release;
	return w;
}

void widget_draw(widget *w)
{
	if(!w)return;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, vid_width, 0, vid_height, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glColor4ub(255,255,255,255);
	glTranslatef(0, vid_height, 0);
	widget_item_draw(w);

}

widget* widget_menu_new(void)
{
	int2 p = {0,0}, s = {200, 30};
	widget *w = widget_new(p, s);
	w->draw = widget_rect_draw;
	return w;
}

widget* widget_menu_add(widget *w, char* label)
{
	if(!w)return 0;
	widget *ret = widget_button_new(0, 0, label);
	ret->parent = w;
	if(w->child)
	{
		ret->pos.x = w->child->pos.x + w->child->size.x;
		ret->next = w->child;
		w->child->prev = ret;
	}
	w->child = ret;
	float x1, x2, y1, y2;
	sth_dim_text(stash, 2, 20.0, label, &x1, &y1, &x2, &y2);
	ret->size.x = 20+ x2 - x1;

	return ret;
}



void font_load(int i, char *path)
{
	int ret = sth_add_font(stash, i, path);
	if(!ret)printf("Failed to load font %d:\"%s\"\n", i, path);
}

int main_init(int argc, char *argv[])
{
	stash = sth_create(512,512);
	if (!stash)
	{
		printf("Could not create stash.\n");
		return -1;
	}
	font_load(0,"data/gui/SourceCodePro-Regular.ttf");
	font_load(1,"data/gui/SourceCodePro-Bold.ttf");
	font_load(2,"data/gui/SourceSansPro-Regular.ttf");
	font_load(3,"data/gui/SourceSansPro-Bold.ttf");


	widget_root = widget_window_new(100, 100, "KITTENS");

	widget *item = widget_button_new(20, 50, "button alpha");
	widget_root->child = item;

	item->next = widget_button_new(20, 120, "button bravo");
	item = item->next;
	item->action_arg = 5;

	widget *menu = widget_menu_new();
	item = widget_menu_add(menu, "File");
//	widget_menu_item_add(item, "Kitten test", spawn_kittens);
//	widget_menu_item_add(item, "Empty button", 0);
	item = widget_menu_add(menu, "Help");
//	widget_menu_item_add(item, "About", 0);

	widget_add(menu);
/*
	int2 pos, size;
	pos.x = 0; pos.y = 0;
	size.x = 200; size.y = 30;
	item = widget_new(pos,size);
	item->next = widget_root;
	
	widget_root = item;
	item->draw = widget_rect_draw;

	size.x = 100; size.y = 30;
	item = widget_button_new(0,0, "File");
	widget_root->child = item;
	item->action_arg = 5;
//	widget *item = 0;
//	item = widget_new(pos, size);
*/


	return 0;
}



void main_loop(void)
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

	widget_draw(widget_root);

	if(keys[KEY_ESCAPE])
		killme=1;

}

void main_end(void)
{
	if(stash)sth_delete(stash);
}

