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


layout(local_size_x = 1, local_size_y = 1) in;


layout(shared, binding=0) buffer atoms { uint fresh, stale; } atom;

layout(shared, binding=1) buffer node_usetime { int i[]; } NodeUseTime;
layout(shared, binding=2) buffer node_lru { int i[]; } NodeLRU;
layout(shared, binding=3) buffer node_lruout { int i[]; } NodeLRUOut;
uniform int time;

void main(void)
{
	uint off = gl_GlobalInvocationID.x * 10;
	off += gl_GlobalInvocationID.y;

	int i = NodeLRU.i[off];
	int node_time = NodeUseTime.i[i];
	if(time == node_time)
	{
		off = atomicAdd(atom.fresh, 1);
		NodeLRUOut.i[off] = i;
	}
	else
	{
		off = atomicAdd(atom.stale, 1);
		NodeLRUOut.i[(NP_SIZE-1) - off] = i;
	}
}

