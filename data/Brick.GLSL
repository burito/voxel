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



layout(rgba8, location=0) uniform writeonly image3D bricks;
layout(shared, binding=0) buffer node_node { int i[][8]; } NodeNode;
layout(shared, binding=1) buffer node_brick { int i[][8]; } NodeBrick;

uniform int depth;

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

	return brick_pos * B_EDGE;
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



void main(void)
{
	vec4 box = vec4(0,0,0,1);

	int node;
	int child;
	int target = 0;
	// search the OctTree
	for(int i=0; i< depth; i++)
	{
		node = target;
		child = oct_child(Position, box);
		target = NodeNode.i[node][child];
		if(0==target)
		{
//			Colour = vec4(0,0,1,1);
//			return;
			discard;
		}
	}
	// find our brick id
	int brick = NodeBrick.i[node][child];
	if(0==brick)
	{
//		Colour = vec4(1,0,0,1);
//		Colour = vec4(Position,1);

//		return;
		discard;
	}
	// find our brick relative coords
	float invbox = 1.0 / box.w;
	vec3 pos = (Position - box.xyz) * invbox;
	pos *= B_SIZE;
	ivec3 ipos = brick_origin(brick) + ivec3(pos);
	// write to the 3D texture
	vec4 colour = vec4(Normal, 1.0);
	imageStore(bricks, ipos, colour);
//	Colour = vec4(box.xyz,1);
//	return;
	discard;
}


