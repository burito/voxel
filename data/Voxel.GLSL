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


uniform sampler3D bricks;
layout(shared) buffer camera { vec4 f[]; } cam;
layout(shared) buffer node_node { int i[][8]; } NodeNode;
layout(shared) buffer node_brick { int i[][8]; } NodeBrick;
layout(shared) buffer node_usetime { int i[][8]; } NodeUseTime;
layout(shared) buffer node_reqtime { int i[][8]; } NodeReqTime;
layout(shared) buffer brick_reqtime { int i[][8]; } BrickReqTime;
layout(shared) buffer brick_usetime { int i[]; } BrickUseTime;

uniform int time;

in vec2 TexCoord;
out vec4 Colour;

#define B_SIZE 8
#define B_EDGE 64
#define B_COUNT (B_EDGE*B_EDGE*B_EDGE)


#define WIGGLE 0.0000001
bool inside(in vec3 pos, in vec4 box)
{
	if(pos.x >= box.x-WIGGLE && pos.x < box.x+box.w+WIGGLE)
	if(pos.y >= box.y-WIGGLE && pos.y < box.y+box.w+WIGGLE)
	if(pos.z >= box.z-WIGGLE && pos.z < box.z+box.w+WIGGLE)
		return true;
	return false;
}

bool slab_ray(in vec3 p, in vec3 inv, in vec4 box)
{
	vec3 near = (box.xyz - p) * inv;
	vec3 far = ((box.xyz+box.w) - p) * inv;
	vec3 tmin = min(near, far), tmax = max(near, far);
	float rmin = max(tmin.x, max(tmin.y, tmin.z));
	float rmax = min(tmax.x, min(tmax.y, tmax.z));
	return (rmax >= rmin) && (rmax >= 0);
}

float slab_exit(in vec3 p, in vec3 inv, in vec4 box)
{
	vec3 near = (box.xyz - p) * inv;
	vec3 far = ((box.xyz+box.w) - p) * inv;
	ivec3 ind = ivec3(step(0.0, inv));
	vec3 ret;
	ret.x = ind.x>0 ? far.x : near.x;
	ret.y = ind.y>0 ? far.y : near.y;
	ret.z = ind.z>0 ? far.z : near.z;
	return min(min(ret.x, ret.y), ret.z);
}

float slab_enter(in vec3 p, in vec3 inv, in vec4 box)
{
	vec3 near = (box.xyz - p) * inv;
	vec3 far = ((box.xyz+box.w) - p) * inv;
	ivec3 ind = ivec3(step(0.0, inv));
	vec3 ret;
	ret.x = ind.x>0 ? near.x : far.x;
	ret.y = ind.y>0 ? near.y : far.y;
	ret.z = ind.z>0 ? near.z : far.z;
	return max(max(ret.x, ret.y), ret.z);
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
	vec2 fpos = TexCoord.xy - 0.5;
	float difference = cam.f[0].w;
	fpos -= difference;	// correct for texel offset
	vec3 p = cam.f[0].xyz;
	vec3 angle = cam.f[1].xyz;
	float fov = cam.f[1].w;

	vec3 n = vec3( sin(fpos.x), sin(fpos.y),
				cos(fpos.x) * cos(fpos.y) );
	// rotate this normal to face the direction of the camera
	vec3 c = cos(angle);
	vec3 s = sin(angle);
	vec3 t;
	t.x = n.x * c.z + n.y * s.z;	// around z
	t.y = n.x * s.z - n.y * c.z;

	n.y = t.y * c.x + n.z * s.x;	// around x
	t.z = t.y * s.x - n.z * c.x;

	n.x = t.x * c.y + t.z * s.y;	// around y
	n.z = t.x * s.y - t.z * c.y;

	vec3 invnorm = 1.0 / n;

	// Are we inside the Octtree?
	vec4 box = vec4(0,0,0,1);
	vec4 colour = vec4(0);

	if(!slab_ray(p, invnorm, box))
	{
		Colour = vec4(0,1,0,1);
		return;
	}
	
	Colour = vec4(0,0,1,1);
	return;

}

