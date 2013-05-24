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
	int fontface;
	float fontsize;
	void (*click)(struct widget*);
	void (*release)(struct widget*);
	void (*onclick)(struct widget*);
	void (*draw)(struct widget*);
	void (*action)(struct widget*);
	int action_arg;
	void *data;
	void *data2;
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
	ret->fontsize = 20.0f;
	ret->fontface = 2;
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
	float dx = 0.0f;
	glColor4f(1,1,1,1);
	sth_begin_draw(stash);
	sth_draw_text(stash, w->fontface, w->fontsize, 10, 10, w->data, &dx);
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
	widget_kill(w);
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
	sth_begin_draw(stash);
	sth_draw_text(stash, w->fontface, w->fontsize,
			10, -(w->size.y/2+5), w->data, 0);
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
		if(w->action)w->action(w);
	}
}




void widget_button_action(widget *w)
{
	printf("Default action\n");
}


widget* widget_button_new(int x, int y, char* label)
{
	int2 p = {x, y}, s = {200 ,30};
	widget *w = widget_new(p, s);
	w->data = label;
	w->draw = widget_button_draw;
	w->onclick = widget_button_onclick;
	w->release = widget_button_release;
//	w->action = widget_button_action;
	return w;
}

widget* widget_text_new(int x, int y, char* label)
{
	int2 p = {x, y}, s = {0 ,30};
	widget *w = widget_new(p, s);
	w->data = label;
	w->draw = widget_text_draw;
	w->noClick = 1;

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
	widget_remove(w);
	widget_add(w);
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
		widget_destroy(w);
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
	sth_begin_draw(stash);
	sth_draw_text(stash, w->fontface, w->fontsize,
			10, -(w->size.y/2+5), w->data, 0);
	sth_end_draw(stash);
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
	float x1, x2, y1, y2;
	sth_dim_text(stash, w->fontface, w->fontsize, label, &x1, &y1, &x2, &y2);
	ret->size.x = 20+ x2 - x1;

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
	
	float x1, x2, y1, y2;
	sth_dim_text(stash, ret->fontface, ret->fontsize,
			label, &x1, &y1, &x2, &y2);
	ret->size.x = 20+ x2 - x1;
	if(wdata->size.x < ret->size.x)wdata->size.x = ret->size.x;

	return ret;
}

void spawn_kittens(widget *x)
{
	widget *w = widget_window_new(100, 100, "KITTENS");
	widget *item = widget_button_new(20, 50, "button alpha");
	widget_child_add(w, item);
	item = widget_button_new(20, 120, "button bravo");
	item->action_arg = 5;
	widget_child_add(w, item);
	widget_add(w);
}


void spawn_homepage(widget *w)
{
	shell_browser("http://danpburke.blogspot.com.au");
}

void spawn_youtube(widget *w)
{
	shell_browser("http://www.youtube.com/user/bur1t0/videos");
}

void spawn_github(widget *w)
{
	shell_browser("https://github.com/burito/voxel");
}

void spawn_about(widget *x)
{
	widget *w = widget_window_new(100, 100, "ABOUT");
	widget *item = widget_text_new(20, 110, "© 2013 Daniel Burke");
	widget_child_add(w, item);
	item = widget_text_new(20, 80, "VOXEL TEST");
	item->fontsize = 40.0f;
	item->fontface = 1;
	widget_child_add(w, item);
	item = widget_button_new(20, 120, "WWW");
	item->size.x = 90;
	item->action = spawn_homepage;
	widget_child_add(w, item);
	item = widget_button_new(120, 120, "GitHub");
	item->action = spawn_github;
	item->size.x = 90;
	widget_child_add(w, item);
	item = widget_button_new(220, 120, "YouTube");
	item->action = spawn_youtube;
	item->size.x = 90;
	widget_child_add(w, item);
	w->size.x = 330;
	w->size.y = 170;
	widget_add(w);
}

void spawn_license(widget *x)
{
	widget *w = widget_window_new(100, 100, "LICENSE");
	widget *item;
	item = widget_text_new(20, 70, "Copyright (c) 2012 Daniel Burke");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 95, "This software is provided 'as-is', without any express or implied");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 110, "warranty. In no event will the authors be held liable for any damages");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 125, "arising from the use of this software.");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 155, "Permission is granted to anyone to use this software for any purpose,");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 170, "including commercial applications, and to alter it and redistribute it");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 185, "freely, subject to the following restrictions:");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 210, "   1. The origin of this software must not be misrepresented; you must not");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 225, "   claim that you wrote the original software. If you use this software");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 240, "   in a product, an acknowledgment in the product documentation would be");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 255, "   appreciated but is not required.");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 280, "   2. Altered source versions must be plainly marked as such, and must not be");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 295, "   misrepresented as being the original software.");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 320, "   3. This notice may not be removed or altered from any source");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	item = widget_text_new(20, 335, "   distribution.");
	item->fontface = 0;	item->fontsize = 14.0f; widget_child_add(w, item);
	w->size.x = 570;
	w->size.y = 360;
	w->pos.x = 20;
	w->pos.y = 80;
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
	sth_begin_draw(stash);
	sth_draw_text(stash, w->fontface, w->fontsize,
			10, -(w->size.y/2+5), (*(int*)w->data2 ? "☑":"☐"), 0);
	sth_draw_text(stash, w->fontface, w->fontsize,
			10, -(w->size.y/2+5), w->data, 0);
	sth_end_draw(stash);
}

void menu_killme(widget *w)
{
	killme = 1;
}

void menu_fullscreen(widget *w)
{
	fullscreen_toggle++;
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

	widget *w;
	widget *menu = widget_menu_new();
	widget *item = widget_menu_add(menu, "File");
	widget_menu_item_add(item, "Kitten test", spawn_kittens);
	widget_menu_item_add(item, "Empty button", 0);
	widget_menu_item_add(item, "Exit", menu_killme);
	item = widget_menu_add(menu, "View");
	w = widget_menu_item_add(item, "     Fullscreen", &menu_fullscreen);
	w->draw = widget_menu_bool_draw;
	w->data2 = &fullscreen;
	
	item = widget_menu_add(menu, "Help");
	widget_menu_item_add(item, "License", spawn_license);
	widget_menu_item_add(item, "About", spawn_about);

	widget_add(menu);

	spawn_about(0);



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

