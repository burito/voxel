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
#define NP_SIZE 100000

layout(local_size_x = 1, local_size_y = 1) in;


layout(binding=0, offset=0) uniform atomic_uint atomFresh;
layout(binding=0, offset=4) uniform atomic_uint atomStale;

layout(shared, binding=1) buffer node_node { int i[]; } NodeNode;
layout(shared, binding=2) buffer node_reqtime { int i[]; } NodeReqTime;
layout(shared, binding=3) buffer node_lru { int i[]; } NodeLRU;
uniform int time;

void main(void)
{
	uint off = gl_GlobalInvocationID.x * 10;
	off = (off+gl_GlobalInvocationID.y) * 8;
	off += gl_GlobalInvocationID.z;

	int node_time = NodeReqTime.i[off];
	if(time != node_time)return;
	int dest = NodeLRU.i[ (NP_SIZE-1) - atomicCounterIncrement(atomStale)];
	NodeNode.i[off] = dest;

	for(int j=0; j<8; j++)
		NodeNode.i[dest*8+j] = 0;
}

