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


layout(local_size_x = 1, local_size_y = 1) in;

layout(binding=0, offset=0) uniform atomic_uint atomFresh;
layout(binding=0, offset=4) uniform atomic_uint atomStale;

layout(rgba8) uniform writeonly image3D bricks;
layout(rgba8) uniform writeonly image3D brick_col;
layout(shared, binding=1) buffer node_brick { int i[]; } NodeBrick;
layout(shared, binding=2) buffer brick_reqtime { int i[]; } BrickReqTime;
layout(shared, binding=3) buffer brick_lru { int i[]; } BrickLRU;
layout(shared, binding=4) buffer brick_loc { uint i[]; } BrickLoc;
uniform int time;


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


void main(void)
{
	uint offset = gl_GlobalInvocationID.x * 10;
	offset = (offset+gl_GlobalInvocationID.y) * 8;
	offset += gl_GlobalInvocationID.z;

	int req_time = BrickReqTime.i[offset];
	if(time != req_time)return;

	int dest = BrickLRU.i[(B_COUNT-1)-atomicCounterIncrement(atomStale)];
	NodeBrick.i[offset] = dest;
	NodeBrick.i[BrickLoc.i[dest]] = 0;
	BrickLoc.i[dest] = offset;

	ivec3 ipos = brick_origin(dest);
	for(int x=0; x<B_SIZE; x++)
	for(int y=0; y<B_SIZE; y++)
	for(int z=0; z<B_SIZE; z++)
	{
		imageStore(bricks, ipos + ivec3(x,y,z), vec4(0));
		imageStore(brick_col, ipos + ivec3(x,y,z), vec4(0));
	}
}

