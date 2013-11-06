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

int gui_init(int argc, char *argv[]);
void gui_input(void);
void gui_draw(void);
void gui_end(void);

/*
 * The saying goes something like...
 * "A good C++ programmer can code C++ in any language"
 *
 * It's not really a class per se, I see it as more of an attack platform.
 * From the widget data structure, one can lay siege to many a UI paradigm.
 */

typedef struct widget
{
	int2 pos, size, delta, drawpos;
	struct widget *parent, *child, *next, *prev;
	int clicked;
	int noClick;
	int noResize;
	int fontface;
	float fontsize;
	void (*click)(struct widget*);
	void (*release)(struct widget*);
	void (*onclick)(struct widget*);
	void (*draw)(struct widget*);
	void (*action)(struct widget*);
	float percent;
	int count;
	int selected;
	void *data;
	void *data2;
	void *data3;
	void (*free)(struct widget*);
} widget;


