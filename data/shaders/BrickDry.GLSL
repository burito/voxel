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

layout(shared, binding=0) buffer node_node { int i[][8]; } NodeNode;
layout(shared, binding=1) buffer node_brick { int i[][8]; } NodeBrick;
layout(shared, binding=2) buffer node_reqtime { int i[][8]; } NodeReqTime;
layout(shared, binding=3) buffer brick_reqtime { int i[][8]; } BrickReqTime;

uniform int time;
uniform int depth;

in vec3 Position;
in vec3 Normal;
in vec2 TexCoord;
out vec4 Colour;

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

/*
Mark only bricks
	* that were requested by the renderer
	* we can draw
Then NodeAlloc and BrickAlloc will be called.
then Brick draw will be called.
*/
void find_brick(in vec3 pos, inout vec4 box)
{
	box = vec4(0,0,0,1);
	int child = oct_child(pos, box);
	int parent = 0;
	int tmp;
	int i;
	for(i=0; i< depth; i++)
	{
		tmp = NodeNode.i[parent][child];
		if(0==tmp)
		{
			if( time-1 == NodeReqTime.i[parent][child])
				if(i < depth-1)
					NodeReqTime.i[parent][child] = time;
			
		
			break;
		}
		parent = tmp;
		child = oct_child(pos, box);
	}
	// find our brick id
	if(i != depth-1) return;
	if( time-1 == BrickReqTime.i[parent][child])
			BrickReqTime.i[parent][child] = time;
}


void brick_mark(in vec3 position, out vec4 box)
{
	find_brick(position, box);
	ivec3 ibpos = ivec3(((position - box.xyz) / box.w)*(B_SIZE-1));
	float boxw = box.w;

	for(int count=0; count<8; count++)
	{
		vec3 posoff = vec3(0);
		if(bool(count & 1))
		{
			if(0 != ibpos.x)continue;
			posoff.x = boxw;
		}
		if(bool(count & 2))
		{
			if(0 != ibpos.y)continue;
			posoff.y = boxw;
		}
		if(bool(count & 4))
		{
			if(0 != ibpos.z)continue;
			posoff.z = boxw;
		}
		
		find_brick(position - posoff, box);
	}

}


void main(void)
{
	vec4 box;
	brick_mark(Position, box);

	discard;
}


