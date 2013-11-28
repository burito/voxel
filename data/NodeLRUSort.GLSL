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


layout(binding=0, offset=0) uniform atomic_uint atomFresh;
layout(binding=0, offset=4) uniform atomic_uint atomStale;

layout(shared) buffer node_usetime { int i[]; } NodeUseTime;
layout(shared) buffer node_lru { int i[]; } NodeLRU;
layout(shared) buffer node_lruout { int i[]; } NodeLRUOut;
uniform int time;

void main(void)
{
	int i = NodeLRU.i[gl_GlobalInvocationID.x];
	int node_time = NodeUseTime.i[i];
	if(time == node_time)
	{
		NodeLRUOut.i[atomicCounterIncrement(atomFresh)] = i;
	}
	else
	{
		NodeLRUOut.i[atomicCounterDecrement(atomStale)] = i;
	}
}

