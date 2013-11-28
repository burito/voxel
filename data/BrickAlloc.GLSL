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


layout(local_size_x = 1, local_size_y = 1) in;

layout(binding=0, offset=0) uniform atomic_uint atomFresh;
layout(binding=0, offset=4) uniform atomic_uint atomStale;

uniform writeonly image3D bricks;
layout(shared) buffer node_brick { int i[]; } NodeBrick;
layout(shared) buffer brick_reqtime { int i[]; } BrickReqTime;
layout(shared) buffer brick_lru { int i[]; } BrickLRU;
uniform int time;

ivec3 brick_origin(in int brick_id)
{
	ivec3 brick_pos;
	int brick_tmp;

	brick_pos.x = brick_id % B_COUNT;
	brick_tmp = (brick_id - brick_pos.x)/B_COUNT;
	brick_pos.y = brick_tmp % B_COUNT;
	brick_pos.z = (brick_tmp - brick_pos.y)/B_COUNT;

	return brick_pos;
}

void main(void)
{
	int i = int(gl_GlobalInvocationID.x);
	int req_time = BrickReqTime.i[i];
	if(time != req_time)return;

	int dest = int(atomicCounterDecrement(atomStale));
	dest = BrickLRU.i[dest];
	NodeBrick.i[i] = dest;
	ivec3 ipos = brick_origin(dest);

	for(int x=0; x<B_SIZE; x++)
	for(int y=0; y<B_SIZE; y++)
	for(int z=0; z<B_SIZE; z++)
		imageStore(bricks, ipos + ivec3(x,y,z), vec4(0));
}

