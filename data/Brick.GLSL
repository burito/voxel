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


#define B_SIZE 8
#define B_EDGE 64
#define B_COUNT (B_EDGE*B_EDGE*B_EDGE)
#define NP_SIZE 100000



layout(rgba16f, location=0) uniform image3D bricks;
layout(shared, binding=0) buffer node_node { int i[][8]; } NodeNode;
layout(shared, binding=1) buffer node_brick { int i[][8]; } NodeBrick;

uniform int depth;
uniform int pass;

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
	int node, child, target = 0;
	for(int i=0; i< depth; i++)
	{
		node = target;
		child = oct_child(pos, box);
		target = NodeNode.i[node][child];
		if(0==target)break;
	}
	// find our brick id
//	if(i == depth) return 0;
	return NodeBrick.i[node][child];
}

void brick_write(in vec3 position, vec4 colour)
{
	vec4 box;
	int brick = find_brick(Position, box);
	float delta = box.w;
	// find our brick relative coords
	vec3 pos = (position - box.xyz) / box.w;
	pos *= (B_SIZE-1);
	ivec3 ibpos = ivec3(pos);
	ivec3 ipos = brick_origin(brick);

	imageStore(bricks, ipos+ibpos, colour);
	if(ibpos.x == 0)
	{
		vec3 tpos = position - vec3(delta,0,0);
		brick = find_brick(tpos, box);
		if(brick > 0)
		{
			ipos = brick_origin(brick);
			ibpos.x = 7;
			imageStore(bricks, ipos+ibpos, colour);
			ibpos.x = 0;
		}
	}
	if(ibpos.y == 0)
	{
		vec3 tpos = position - vec3(0, delta, 0);
		brick = find_brick(tpos, box);
		if(brick > 0)
		{
			ipos = brick_origin(brick);
			ibpos.y = 7;
			imageStore(bricks, ipos+ibpos, colour);
			ibpos.y = 0;
		}
	}
	if(ibpos.z == 0)
	{
		vec3 tpos = position - vec3(0, 0, delta);
		brick = find_brick(tpos, box);
		if(brick > 0)
		{
			ipos = brick_origin(brick);
			ibpos.z = 7;
			imageStore(bricks, ipos+ibpos, colour);
			ibpos.z = 0;
		}
	}

}

void main(void)
{
	vec4 colour = vec4(Normal, 1.0);
	brick_write(Position, colour);
	discard;
/*
	vec4 front, back;
	int stepdir = -1;
	if(Normal.x > 0) stepdir = 1;
	for(int i=int(pos.x); i<B_SIZE, i>-1; i+=stepdir)
	{

		vec4 colour_in = imageLoad(bricks, ipos);
		if(colour_in.w > 0.1) break;
	
		imageStore(bricks, ipos, back);
		ipos.x += stepdir;
	}
	discard;
	return;
*/
}


