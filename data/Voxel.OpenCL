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




/*

brickpool is 512^3
nodepool is 100000
*/

#define B_SIZE 8
#define B_EDGE 64
#define B_COUNT (B_EDGE*B_EDGE*B_EDGE)

#define NP_SIZE 100000



#define WIGGLE 0.0000001
bool inside(float3 pos, float4 box)
{
	if(pos.x >= box.x-WIGGLE && pos.x < box.x+box.w+WIGGLE)
	if(pos.y >= box.y-WIGGLE && pos.y < box.y+box.w+WIGGLE)
	if(pos.z >= box.z-WIGGLE && pos.z < box.z+box.w+WIGGLE)
		return true;
	return false;
}


bool slab_ray(float3 p, float3 in, float4 box)
{
	float3 near = (box.xyz - p) * in;
	float3 far = ((box.xyz+box.w) - p) * in;
	float3 tmin = min(near, far), tmax = max(near, far);
	float rmin = max(tmin.x, max(tmin.y, tmin.z));
	float rmax = min(tmax.x, min(tmax.y, tmax.z));
	return (rmax >= rmin) && (rmax >= 0);
}

// original slab_exit() from 
// http://www.opengl.org/discussion_boards/showthread.php/176344-Ray-vs-AABB-exit-point-knowing-entry-point

float slab_exit(float3 p, float3 in, float4 box)
{
	float3 near = (box.xyz - p) * in;
	float3 far = ((box.xyz+box.w) - p) * in;
	int3 ind = convert_int(step(0.0, in));
	float3 ret;
	ret.x = ind.x ? far.x : near.x;
	ret.y = ind.y ? far.y : near.y;
	ret.z = ind.z ? far.z : near.z;
	return min(min(ret.x, ret.y), ret.z);
}

float slab_enter(float3 p, float3 in, float4 box)
{
	float3 near = (box.xyz - p) * in;
	float3 far = ((box.xyz+box.w) - p) * in;
	int3 ind = convert_int(step(0.0, in));
	float3 ret;
	ret.x = ind.x ? near.x : far.x;
	ret.y = ind.y ? near.y : far.y;
	ret.z = ind.z ? near.z : far.z;
	return max(max(ret.x, ret.y), ret.z);
}


int oct_child(float3 pos, float4 *vol)
{
	int child=0;
	vol->w *= 0.5;
	float3 mid = vol->xyz + vol->w;
	if(pos.x >= mid.x)
	{
		vol->x +=vol->w;
		child += 1;
	}

	if(pos.y >= mid.y)
	{
		vol->y += vol->w;
		child += 2;
	}

	if(pos.z >= mid.z)
	{
		vol->z += vol->w;
		child += 4;
	}
	return child;
}

uint find_brick(
			__global uint NodeNode[][8],
			__global uint NodeBrick[][8],
			__global float NodeUseTime[][8],
			__global float NodeReqTime[][8],
			__global float BrickReqTime[][8],
			__global float BrickUseTime[],
			float3 pos,
			float4 *box,
			float size,
			float time)
{
	uint child = 0;
	uint index = 0;
	uint tmp;
	float volume = 1.0/(float)B_SIZE;

	while(volume > size)
	{
		volume *= 0.5;
		child = oct_child(pos, box);
		NodeUseTime[index][child] = time;
		tmp = NodeNode[index][child];
		if(tmp==0)
		{
			NodeReqTime[index][child] = time;
			break;
		}
		if(tmp & 0x8000000)break;
		index = tmp;
	}

	tmp = NodeBrick[index][child];
	if(tmp)BrickUseTime[tmp] = time;
	return tmp;
}

float3 brick_origin(uint brick_id)
{
	float3 brick_pos;
	uint brick_tmp;
	brick_pos.x = (float)(brick_id % B_EDGE);
	brick_tmp = (brick_id - (uint)brick_pos.x)/B_EDGE;
	brick_pos.y = (float)(brick_tmp % B_EDGE);
	brick_pos.z = (float)((brick_tmp - (uint)brick_pos.y)/B_EDGE);
	return brick_pos / (float)B_EDGE;
}


void brick_sample(
			__read_only image3d_t bricks,
			uint brick_id,
			float3 near,
			float3 far,
			float4 *colour,
			float *steprem)
{
	float iSize = 1.0 / (float)B_SIZE;
	float iCount = 1.0 / (float)B_EDGE;
	float hs = iSize * 0.5;
	near = near * (iSize * (B_SIZE-1)) + (float3)hs;
	far = far * (iSize * (B_SIZE-1)) + (float3)hs;
	float len = length(far - near);
	float3 brick_pos = brick_origin(brick_id);
	float4 result = *colour;

	const sampler_t brSamp = CLK_NORMALIZED_COORDS_TRUE |
							 CLK_ADDRESS_CLAMP_TO_EDGE |
							 CLK_FILTER_LINEAR;

	if(len < 0.1)return;

	float steps = 1.0/(len / iSize);

	float pos;
	for(pos= *steprem * steps; pos < 1.0; pos += steps)
	{	
		float3 sample_loc = mix(near, far, pos)*iCount;
		float4 voxel = read_imagef(bricks, brSamp,
			(float4)(sample_loc+brick_pos, 0));
		result.xyz += ((1.0-result.w)*(1.0-result.w) * voxel.xyz)/(1.0 - result.xyz * voxel.xyz);
		result.w = result.w + (1.0-result.w) * voxel.w;
//		result.xyz = result.xyz + (1.0-result.w) * voxel.xyz;
//		result.w = result.w + (1.0-result.w) * voxel.w;
		if(result.w > 1.0)break;
	}
	*steprem = (pos - 1.0) /steps;
	*colour = result;
}


// we know we're inside the volume
float3 escape(float3 pos, float3 normal, float4 box)
{
	float3 far;
	far.x = normal.x < 0 ? box.x : box.x + box.w;
	far.y = normal.y < 0 ? box.y : box.y + box.w;
	far.z = normal.z < 0 ? box.z : box.z + box.w;

	if(normal.x == 0) normal.x += 0.00001;
	if(normal.y == 0) normal.y += 0.00001;
	if(normal.z == 0) normal.z += 0.00001;

	float3 ratio = (far - pos)/normal;
	float rmin = min(min(ratio.x, ratio.y), ratio.z);
	return rmin * normal + pos;
}



__kernel void Render(
			__write_only image2d_t out,
			__read_only image3d_t bricks,
			__global float4 *camera,
			const float time,
			__global uint NodeNode[][8],
			__global uint NodeBrick[][8],
			__global float NodeUseTime[][8],
			__global float NodeReqTime[][8],
			__global float BrickReqTime[][8],
			__global float* BrickUseTime)
{
	int2 ipos = {get_global_id(0), get_global_id(1)};
	float2 size = {get_global_size(0), get_global_size(1)};
	float2 fpos = convert_float(ipos);

	float3 p = camera[0].xyz;
	float3 angle = camera[1].xyz;
	float fov = camera[1].w;
	float pfov = sin(fov / size.x);		// pixel fov
	float psize = 0.0;					// pixel size
	// Translate OpenCL coordinates into per pixel normal
	float2 srat = (float2)(0.5, (size.y / size.x) *0.5);
	fpos = (fpos / size.x) - srat;
	float3 n = (float3)( sin(fpos.x), sin(fpos.y),
				cos(fpos.x) * cos(fpos.y) );
	// rotate this normal to face the direction of the camera
	float3 c = cos(angle);
	float3 s = sin(angle);
	float3 t;
	t.x = n.x * c.z + n.y * s.z;	// around z
	t.y = n.x * s.z - n.y * c.z;

	n.y = t.y * c.x + n.z * s.x;	// around x
	t.z = t.y * s.x - n.z * c.x;

	n.x = t.x * c.y + t.z * s.y;	// around y
	n.z = t.x * s.y - t.z * c.y;

	float3 in = 1.0 / n;

	// Are we inside the Octtree?
	float4 box = {0,0,0,1};
	float4 colour = {0,0,0,0};

	if(!slab_ray(p, in, box))
	{
		colour = (float4)(0,1,0,1);
		write_imagef(out, ipos, colour);
		return;
	}

	if(!inside(p, box))
	{
		float dist = slab_enter(p, in, box);
		p += n * dist;
		psize += pfov * dist;
		colour = (float4)(0,0,1,1);
	write_imagef(out, ipos, colour);

	}
	return;
	// p is inside the volume, which brick?
	uint brick_id = find_brick(NodeNode, NodeBrick, NodeUseTime,
		NodeReqTime, BrickReqTime, BrickUseTime, p, &box, psize, time);
	float steprem = 0;		// step remainder

	// loop until we hit something solid/exit the volume
	for(int i=0; i<50 && colour.w < 1.0; i++)
	{
		float dist = slab_exit(p, in, box);
		float3 far = dist * n;
		if(brick_id != 0)
		{
			float boxinvw = 1.0 / box.w;
			float3 bnear = (p - box.xyz) * boxinvw;
			float3 bfar = (far - box.xyz) * boxinvw;
			brick_sample(bricks, brick_id, bnear, bfar, &colour, &steprem);
		}
		else steprem = 0;

		if(!inside(far, (float4)(0,0,0,1)))break;
		brick_id = find_brick(NodeNode, NodeBrick, NodeUseTime,
			NodeReqTime, BrickReqTime, BrickUseTime, p, &box, psize, time);

	}

	write_imagef(out, ipos, colour);
}

__kernel void ResetNodeTime(
			const float time,
			__global float NodeUseTime[][8],
			__global float NodeReqTime[][8],
			__global float BrickReqTime[][8])
{
	int i = get_global_id(0);
	for(int j=0; j< 8; j++)
	{
		NodeUseTime[i][j] = time;
		NodeReqTime[i][j] = time;
		BrickReqTime[i][j] = time;
	}
}

__kernel void ResetBrickTime(const float time, __global float* BrickUseTime)
{
	int i = get_global_id(0);
	BrickUseTime[i] = time;
}


