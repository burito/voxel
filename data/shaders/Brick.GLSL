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

#version 430

#define B_COUNT (B_EDGE*B_EDGE*B_EDGE)

layout(rgba16f, location=0) uniform image3D bricks;
layout(shared, binding=0) buffer node_node { int i[][8]; } NodeNode;
layout(shared, binding=1) buffer node_brick { int i[][8]; } NodeBrick;
layout(shared, binding=2) buffer brick_reqtime { int i[][8]; } BrickReqTime;

uniform int depth;
uniform int time;

//in bool gl_FrontFacing;
in vec3 Position;
in vec3 Normal;
in vec2 TexCoord;
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

void brick_write(in vec3 position, vec4 colour)
{
	vec4 box;
	int brick = find_brick(position, box);
	if(0 == brick)return;
	ivec3 ibpos = ivec3(((position - box.xyz) / box.w)*(B_SIZE-1));
	float boxw = box.w;
	
	ivec3 ipos = brick_origin(brick);

	imageStore(bricks, ipos+ibpos, colour);
	if(ibpos.x == 0)
	{
		vec3 tpos = position - vec3(boxw,0,0);
		brick = find_brick(tpos, box);
		if(brick > 0)
		{
			ipos = brick_origin(brick);
			ibpos.x = B_SIZE-1;
			imageStore(bricks, ipos+ibpos, colour);
			ibpos.x = 0;
		}
	}
	if(ibpos.y == 0)
	{
		vec3 tpos = position - vec3(0, boxw, 0);
		brick = find_brick(tpos, box);
		if(brick > 0)
		{
			ipos = brick_origin(brick);
			ibpos.y = B_SIZE-1;
			imageStore(bricks, ipos+ibpos, colour);
			ibpos.y = 0;
		}
	}
	if(ibpos.z == 0)
	{
		vec3 tpos = position - vec3(0, 0, boxw);
		brick = find_brick(tpos, box);
		if(brick > 0)
		{
			ipos = brick_origin(brick);
			ibpos.z = B_SIZE-1;
			imageStore(bricks, ipos+ibpos, colour);
			ibpos.z = 0;
		}
	}
}

void main(void)
{
	
	vec4 colour = vec4(Normal, 1.0);
	brick_write(Position, colour);
/*	vec3 absNorm = abs(Normal);
	if(absNorm.x > absNorm.y)
	{
		if(absNorm.x > absNorm.z)
		{
			delta *= sign(Normal.x);
			brick_write(Position+vec3(delta,0,0), colour);
		}
		else
		{
			delta *= sign(Normal.z);
			brick_write(Position+vec3(0,0,delta), colour);
		}

	}
	else
	{
		if(absNorm.y > absNorm.z)
		{
			delta *= sign(Normal.y);
			brick_write(Position+vec3(0,delta,0), colour);


		}
		else
		{
			delta *= sign(Normal.z);
			brick_write(Position+vec3(0,0,delta), colour);
		}

	}
*/
	discard;
}


