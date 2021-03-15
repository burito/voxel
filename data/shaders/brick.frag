#version 430
#extension GL_ARB_shading_language_include : require
#include "/voxel_data"
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

#define B_COUNT (B_EDGE*B_EDGE*B_EDGE)

uniform sampler2D tex;

layout(rgba8, location=4) uniform image3D bricks ;
layout(rgba8, location=5) uniform image3D brick_col;
layout(shared, binding=0) buffer node_node { int i[][8]; } NodeNode;
layout(shared, binding=1) buffer node_brick { int i[][8]; } NodeBrick;
layout(shared, binding=2) buffer brick_reqtime { int i[][8]; } BrickReqTime;

uniform int depth;
uniform int time;
uniform int pass;

//in bool gl_FrontFacing;
layout (location = 0) in vec3 fragPosition;
layout (location = 1) noperspective in vec2 fragUV;
layout (location = 2) in vec3 fragNormal;
//in vec4 vColour;
out vec4 Colour;

ivec3 brick_origin(in int brick_id)
{
	ivec3 brick_pos;
	int brick_tmp;

	brick_pos.x = brick_id % B_EDGE;
	brick_tmp = (brick_id - brick_pos.x)/B_EDGE;
	brick_pos.y = brick_tmp % B_EDGE;
	brick_pos.z = (brick_tmp - brick_pos.y)/B_EDGE;

	return brick_pos * B_SIZE;
}

int oct_child(in vec3 pos, inout vec4 vol)
{
	int child=0;
	vol.w *= 0.5;
	vec3 mid = vol.xyz + vol.w;
	if(pos.x >= mid.x)
	{
		vol.x +=vol.w;
		child += 1;
	}

	if(pos.y >= mid.y)
	{
		vol.y += vol.w;
		child += 2;
	}

	if(pos.z >= mid.z)
	{
		vol.z += vol.w;
		child += 4;
	}
	return child;
}

int find_brick(in vec3 pos, inout vec4 box)
{
	box = vec4(0,0,0,1);
	int child = oct_child(pos, box);
	int parent = 0;
	int tmp;
	int i;
	for(i=0; i< depth; i++)
	{
		tmp = NodeNode.i[parent][child];
		if(0==tmp)break;
		parent = tmp;
		child = oct_child(pos, box);
	}
	// find our brick id
	if(i != depth-1) return 0;
	if(BrickReqTime.i[parent][child] < time)return 0;
	return NodeBrick.i[parent][child];
}

void brick_write(in vec3 position, vec4 colour, vec4 normal)
{
	vec4 box;
	int brick = find_brick(position, box);
	if(0 == brick)return;
	ivec3 ibpos = ivec3(((position - box.xyz) / box.w)*float(B_SIZE-1));
	float boxw = box.w;
	ivec3 orig_ibpos = ibpos;

	for(int count=0; count<8; count++)
	{
		vec3 posoff = vec3(0);
		ibpos = orig_ibpos;
		if(bool(count & 1))
		{
			if(0 != ibpos.x)continue;
			ibpos.x = B_SIZE-1;
			posoff.x = boxw;
		}
		if(bool(count & 2))
		{
			if(0 != ibpos.y)continue;
			ibpos.y = B_SIZE-1;
			posoff.y = boxw;
		}
		if(bool(count & 4))
		{
			if(0 != ibpos.z)continue;
			ibpos.z = B_SIZE-1;
			posoff.z = boxw;
		}

		brick = find_brick(position - posoff, box);
		if(0 == brick)continue;
		ivec3 ipos = brick_origin(brick);
		imageStore(bricks, ipos+ibpos, normal);
		imageStore(brick_col, ipos+ibpos, colour);
	}

}

void main(void)
{
	vec4 normal = vec4(fragNormal, 1.0);
	vec4 colour;
	ivec2 s = textureSize(tex,0);
	if(s.x > 10)
	{
		colour = texture(tex, fragUV);
	}
	else
	{
		colour = vec4(1);
	}
//	brick_write(fragPosition, colour, normal);
	vec4 box;
	int brick = find_brick(fragPosition, box);
	if(0 == brick)return;
	float delta = (box.w / float(B_SIZE-1));
	vec3 bpos = ((fragPosition - box.xyz) / box.w)*float(B_SIZE-1);
	ivec3 ibpos = ivec3(bpos);
	vec3 palpha = bpos - vec3(ibpos);

	vec3 vsize = sign(fragNormal) * delta;
	vec3 vstep = vec3(0);

	switch(pass) {
	case 0:
		normal.w = palpha.x;
		vstep.x = vsize.x;
		break;
	case 1:
		normal.w = palpha.y;
		vstep.y = vsize.y;
		break;
	case 2:
		normal.w = palpha.z;
		vstep.z = vsize.z;
		break;
	}

	brick_write(fragPosition, colour, normal);
	normal.w = 1;
	brick_write(fragPosition+vstep, colour, normal);
	brick_write(fragPosition+vstep*2.0, colour, normal);

/*	vec3 absNorm = abs(fragNormal);
	if(absNorm.x > absNorm.y)
	{
		if(absNorm.x > absNorm.z)
		{
			delta *= sign(fragNormal.x);
			brick_write(fragPosition+vec3(delta,0,0), colour, normal);
		}
		else
		{
			delta *= sign(fragNormal.z);
			brick_write(fragPosition+vec3(0,0,delta), colour, normal);
		}

	}
	else
	{
		if(absNorm.y > absNorm.z)
		{
			delta *= sign(fragNormal.y);
			brick_write(fragPosition+vec3(0,delta,0), colour, normal);


		}
		else
		{
			delta *= sign(fragNormal.z);
			brick_write(fragPosition+vec3(0,0,delta), colour, normal);
		}

	}
*/
	discard;
}
