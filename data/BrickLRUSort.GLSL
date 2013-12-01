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

#define B_EDGE 64
#define B_COUNT (B_EDGE*B_EDGE*B_EDGE)

layout(local_size_x = 1, local_size_y = 1) in;


layout(binding=0, offset=0) uniform atomic_uint atomFresh;
layout(binding=0, offset=4) uniform atomic_uint atomStale;

layout(shared, binding=1) buffer brick_usetime { int i[]; } BrickUseTime;
layout(shared, binding=2) buffer brick_lru { int i[]; } BrickLRU;
layout(shared, binding=3) buffer brick_lruout { int i[]; } BrickLRUOut;
uniform int time;

void main(void)
{
	uint off = gl_GlobalInvocationID.x * 64;
	off = (off+gl_GlobalInvocationID.y)* 64;
	off += gl_GlobalInvocationID.z;

	int i = BrickLRU.i[off];
	int brick_time = BrickUseTime.i[i];
	if(time == brick_time)
	{
		off = atomicCounterIncrement(atomFresh);
		BrickLRUOut.i[off] = i;
	}
	else
	{
		off = atomicCounterIncrement(atomStale);
		BrickLRUOut.i[(B_COUNT-1) - off] = i;
	}
}

