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

#include <GL/glew.h>

#include "3dmaths.h"
#include "image.h"
#include "main.h"

#include "fontstash.h"

struct sth_stash* stash = 0;

typedef struct widget
{
	int2 pos, size, delta;
	struct widget *child, *next;
	int clicked;
	int noClick;
	void (*click)(struct widget*);
	void (*release)(struct widget*);
	void (*onclick)(struct widget*);
	void (*draw)(struct widget*);
	void *data;
} widget;

widget *widget_root = 0;


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

void widget_kill(widget* root)
{
	if(!root)return;
	widget_kill(root->child);
	widget_kill(root->next);
//	if(root->data)free(root->data);
	free(root);
}


widget* widget_shoot(widget *root, int2 pos)
{
	if(!root)return 0;
	if(root->noClick)return 0;
	if((pos.x > root->pos.x && pos.x < (root->pos.x + root->size.x)) &&
	   (pos.y > root->pos.y && pos.y < (root->pos.y + root->size.y)))
	{

		widget *ret = 0;
		int2 relative = { pos.x - root->pos.x, pos.y - root->pos.y };
		ret = widget_shoot(root->child, relative);
		if(ret)return ret;
		return root;
	}
	else return widget_shoot(root->next, pos);
}


void widget_item_draw(widget *root)
{
	if(!root)return;
	widget_item_draw(root->next);
	glTranslatef((GLfloat)root->pos.x, (GLfloat)-root->pos.y, 0.0f);
	if(root->draw)root->draw(root);
	widget_item_draw(root->child);
	glTranslatef((GLfloat)-root->pos.x, (GLfloat)root->pos.y, 0.0f);
}

void widget_draw_text(widget *root)
{
	if(!root)return;
	float dx = 0.0f;
	sth_begin_draw(stash);
	sth_draw_text(stash, 0,24.0f, 0, 0, root->data, &dx);
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


void widget_button_draw(widget *root)
{
	float colour = 0.0f;
	if(root->clicked)colour = 0.3f;
	glColor4f(colour, colour, colour, 0.4f);
	draw_rect(root->size.x, root->size.y);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	sth_begin_draw(stash);
	sth_draw_text(stash, 0,24.0f, 20, -31, root->data, 0);
	sth_end_draw(stash);
}

void widget_button_onclick(widget *root)
{
	root->clicked = 1;
	printf("clicked %s\n", root->data);
}

void widget_draw_window(widget *root)
{
	GLfloat colour=0.4f;
	glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
	draw_rect(root->size.x, root->size.y);
	glColor4f(colour, colour, colour, 1.0f);
	glTranslatef(0, -5, 0);
	draw_rect(root->size.x, -3);

//	glTranslatef(0, -30, 0);

	if(3 == root->clicked)
	{
		glColor4f(0.5f, 0.5f, 0.2f, 0.4f);
		glTranslatef(root->size.x, 0, 0);
		draw_rect(-50, 23);
		glTranslatef(-root->size.x, 0, 0);

	}
	glTranslatef(0, -25, 0);

	glColor4f(1,1,1,1);
	sth_begin_draw(stash);
	sth_draw_text(stash, 1,24.0f, 20, 7, root->data, 0);
	sth_draw_text(stash, 0,14.0f, root->size.x - 35, 10, "END", 0);
	sth_end_draw(stash);
//	glTranslatef(0, 0, 0);
	glColor4f(colour, colour, colour, 1.0f);
	draw_rect(root->size.x, -2);
	glTranslatef(0, 30, 0);

}

void widget_window_click(widget *root)
{
	if(2 == root->clicked)		// dragging
	{
		root->pos.x -= mickey_x;
		root->pos.y -= mickey_y;
	}

}

void widget_window_onclick(widget *root)
{
	root->delta.x = mouse_x - root->pos.x;
	root->delta.y = mouse_y - root->pos.y;

	if(root->delta.y > 5)
	if(root->delta.y < 30)
	{
		if( root->delta.x > (root->size.x - 50) )
				root->clicked=3;	// end
		else
			root->clicked=2;		// dragging
	}


}

void widget_window_release(widget *root)
{
	if(3 != root->clicked)return;
	root->delta.x = mouse_x - root->pos.x;
	root->delta.y = mouse_y - root->pos.y;
	if(root->delta.y > 5)
	if(root->delta.y < 30)
	if( root->delta.x > (root->size.x - 50) )
	{
		printf("end\n");
	}

}

void widget_draw(widget *root)
{
	if(!root)return;
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
	widget_item_draw(root);
}


int main_init(int argc, char *argv[])
{
	stash = sth_create(512,512);
	if (!stash)
	{
		printf("Could not create stash.\n");
		return -1;
	}
	if (!sth_add_font(stash,0,"data/gui/SourceCodePro-Regular.ttf"))
	{
		printf("Could not add font.\n");
		return -1;
	}
	if (!sth_add_font(stash,1,"data/gui/SourceCodePro-Bold.ttf"))
	{
		printf("Could not add font.\n");
		return -1;
	}	


	int2 zero = {0, 0};
	int2 pos = {100, 100};
	int2 size = {400, 300};
	
	widget_root = widget_new(pos, size);
	widget_root->draw = widget_draw_window;
	widget_root->data = "KITTENS";
	widget_root->click = widget_window_click;
	widget_root->onclick = widget_window_onclick;
	widget_root->release = widget_window_release;


	pos.x = 20; pos.y = 50;
	size.x = 200; size.y = 50;
	widget *item = widget_new(pos,size);
	widget_root->child = item;
	item->data = "button alpha";
	item->draw = widget_button_draw;
	item->onclick = widget_button_onclick;

	pos.x = 20; pos.y = 120;
	item->next = widget_new(pos,size);
	item = item->next;
	item->data = "button bravo";
	item->draw = widget_button_draw;
	item->onclick = widget_button_onclick;

//	widget *item = 0;
//	item = widget_new(pos, size);



	return 0;
}





widget *latched = 0;

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

