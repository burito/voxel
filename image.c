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

#include <string.h>
#include <stdlib.h>

#include <GL/glew.h>
#include "stb_image.h"

#include "image.h"
#include "text.h"


void img_glinit(IMG *img)
{
	int type = 0, intfmt = 0;

	glEnable(GL_TEXTURE_2D);
//	glEnable(GL_COLOR_TABLE);

	glGenTextures(1, &img->id);
	glBindTexture(GL_TEXTURE_2D, img->id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);


	switch(img->channels) {
	case 4: type = GL_RGBA; intfmt = GL_RGBA8; break;
	case 3: type = GL_RGB; intfmt = GL_RGB8; break;
	case 2: type = GL_LUMINANCE_ALPHA; intfmt = GL_LUMINANCE8_ALPHA8; break;
	case 1: type = GL_LUMINANCE; intfmt = GL_LUMINANCE8; break;
	default: type = GL_NONE;
	}
	
//		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//		glPixelTransferi(GL_INDEX_SHIFT, 8);
//		glColorTable(GL_COLOR_TABLE, GL_RGB8, 256, GL_RGB, GL_UNSIGNED_BYTE, img->palette);

/*	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	gluBuild2DMipmaps( GL_TEXTURE_2D, t->bpp, t->x, t->y, t->type,
				GL_UNSIGNED_BYTE, t->buf );
*/

	glTexImage2D(GL_TEXTURE_2D, 0, intfmt, img->x, img->y,
		0, type, GL_UNSIGNED_BYTE, img->buf);
	glBindTexture(GL_TEXTURE_2D, 0);
}


void img_free(IMG *img)
{
	if(img->id)glDeleteTextures(1, &img->id);
	if(img->name)free(img->name);
	if(img->buf)stbi_image_free(img->buf);
	free(img);
}


IMG* img_load(const char * filename)
{
	printf("Loading Image(\"%s\")\n", filename);
	const int size = sizeof(IMG);
	IMG *i = malloc(size);
	if(!i)return 0;
	memset(i, 0, size);
	i->name = hcopy(filename);
	i->buf = stbi_load(filename, &i->x, &i->y, &i->channels, 0);
	img_glinit(i);
	return i;
}


#ifdef ISTANDALONE

int main(int argc, char *argv[])
{
	img_buf *i;

	if(!argv[1])
	{
		printf("USAGE: %s name\n\t reads name where name is an image.\n",
				argv[0]);
		return 1;
	}

	printf("Loading file \"%s\"\n", argv[1]);

	i = img_load(argv[1]);

	if(!i)
	{
		printf("Failed to load.\n");
		return 2;
	}

	img_free(i);
	printf("Happily free.\n");
	return 0;
}

#endif

