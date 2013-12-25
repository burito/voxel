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

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
//#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

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
	float3 far = (box.xyz+box.w - p) * in;
	float3 tmin = min(near, far), tmax = max(near, far);
	float rmin = max(tmin.x, max(tmin.y, tmin.z));
	float rmax = min(tmax.x, min(tmax.y, tmax.z));
	return (rmax >= rmin) && (rmax >= 0);
}

float slab_exitside(float3 p, float3 in, float4 box, float3 *axis)
{
	float3 ret, nearfar[2];
	nearfar[0] = (box.xyz - p) * in;
	nearfar[1] = (box.xyz+box.w - p) * in;
	int3 ind = convert_int(step(0.0, in));
	ret.x = nearfar[ind.x].x;
	ret.y = nearfar[ind.y].y;
	ret.z = nearfar[ind.z].z;
	ind = ind * 2 - 1;
	float r;
	if(ret.x < ret.y)
	{
		r = ret.x;
		*axis = (float3)((float)ind.x,0,0);
	}
	else
	{	
		r = ret.y;
		*axis = (float3)(0,(float)ind.y,0);
	}
	if( ret.z < r)
	{
		r = ret.z;
		*axis = (float3)(0,0,(float)ind.z);
	}
	return r;
}


// original slab_exit() from 
// http://www.opengl.org/discussion_boards/showthread.php/176344-Ray-vs-AABB-exit-point-knowing-entry-point

float slab_exit(float3 p, float3 in, float4 box)
{
	float3 ret, nearfar[2];
	nearfar[0] = (box.xyz - p) * in;
	nearfar[1] = (box.xyz+box.w - p) * in;
	int3 ind = convert_int(step(0.0, in));
	ret.x = nearfar[ind.x].x;
	ret.y = nearfar[ind.y].y;
	ret.z = nearfar[ind.z].z;
	return min(min(ret.x, ret.y), ret.z);
}


float slab_enter(float3 p, float3 in, float4 box)
{
	float3 ret, nearfar[2];
	nearfar[0] = (box.xyz - p) * in;
	nearfar[1] = (box.xyz+box.w - p) * in;
	int3 ind = convert_int(step(0.0, -in));
	ret.x = nearfar[ind.x].x;
	ret.y = nearfar[ind.y].y;
	ret.z = nearfar[ind.z].z;
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

int find_brick(
			__global int NodeNode[][8],
			__global int NodeBrick[][8],
			__global int NodeUseTime[],
			__global int NodeReqTime[][8],
			__global int BrickReqTime[][8],
			__global int BrickUseTime[],
			float3 pos,
			float4 *box,
			float size,
			int time)
{
	*box = (float4)(0,0,0,1);
	int child = oct_child(pos, box);
	int parent = 0;
	int tmp;
	float volume = size * (float)B_SIZE;

	while(volume < box->w)
	{
		NodeUseTime[parent] = time;
		tmp = NodeNode[parent][child];
		if(tmp==0)
		{
			NodeReqTime[parent][child] = time;
			break;
		}
		if(tmp < 0)return 0;
		parent = tmp;
		child = oct_child(pos, box);
	}

	tmp = NodeBrick[parent][child];
	if(0==tmp)
	{
		BrickReqTime[parent][child] = time;
	}
	else
	{
		BrickUseTime[tmp] = time;
	}
	return tmp;
}

float3 brick_origin(int brick_id)
{
	float3 brick_pos;
	uint brick_tmp;
	brick_pos.x = (float)(brick_id % B_EDGE);
	brick_tmp = (brick_id - (int)brick_pos.x)/B_EDGE;
	brick_pos.y = (float)(brick_tmp % B_EDGE);
	brick_pos.z = (float)((brick_tmp - (int)brick_pos.y)/B_EDGE);
	return brick_pos / (float)B_EDGE;
}

int3 brick_origini(int brick_id)
{
	int3 brick_pos;
	int brick_tmp;
	brick_pos.x = brick_id % B_EDGE;
	brick_tmp = (brick_id - brick_pos.x)/B_EDGE;
	brick_pos.y = (brick_tmp % B_EDGE);
	brick_pos.z = (brick_tmp - brick_pos.y)/B_EDGE;
	return brick_pos;
}


void brick_ray(
			__read_only image3d_t bricks,
			int brick_id,
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

	float steps = 0.5/(len / iSize);

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
			const int time,
			__global int NodeNode[][8],
			__global int NodeBrick[][8],
			__global int NodeReqTime[][8],
			__global int BrickReqTime[][8],
			__global int NodeUseTime[],
			__global int BrickUseTime[])
{
	int2 ipos = {get_global_id(0), get_global_id(1)};
	float2 size = {get_global_size(0), get_global_size(1)};
	float2 fpos = convert_float(ipos);

	float3 p = camera[0].xyz;
	float3 angle = camera[1].xyz;
	float fov = camera[1].w;
	float prat = sin(fov / size.x);		// pixel ratio
	// Translate OpenCL coordinates into per pixel normal
	float2 srat = (float2)(0.5, (size.y / size.x) *0.5);
	fpos = (fpos / size.x) - srat;
	fpos *= fov;
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

	n = normalize(n);
	float3 invnorm = 1.0 / n;
	float3 normsign = sign(n);

	// Are we inside the Octtree?
	float4 box = {0,0,0,1};
	float dist = 0;
	float psize = 0;
	float3 far, near = p;
	if(!inside(near, box))
	{
		if(!slab_ray(near, invnorm, box))	// will we enter the block?
		{
			write_imagef(out, ipos, (float4)(0,0,0,0));
			return;
		}
		dist = slab_enter(near, invnorm, box);
		psize = dist * prat;
		near += n * dist;
	}
	// p is inside the volume, which brick?
	int brick_id = find_brick(NodeNode, NodeBrick, NodeUseTime,
		NodeReqTime, BrickReqTime, BrickUseTime, near, &box, psize, time);
	float steprem = 0;		// step remainder

	// loop until we hit something solid/exit the volume
	float4 colour = {0.0,0.0,0.0,0.0};
	int i, ib=0;
	for(i=0; i<50 && colour.w < 1.0; i++)
	{
		float3 axis;
		dist = slab_exitside(near, invnorm, box, &axis);
		far = near + dist * n;
		psize += dist * prat;
		if(brick_id != 0)
		{
			float3 bnear = (near - box.xyz) / box.w;
			float3 bfar = (far - box.xyz) / box.w;
			brick_ray(bricks, brick_id, bnear, bfar, &colour, &steprem);
			ib++;
		}
		else steprem = 0;


		float3 tmp = far + axis*(box.w * 0.1);
		if(!inside(tmp, (float4)(0,0,0,1)))break;
		brick_id = find_brick(NodeNode, NodeBrick, NodeUseTime,
			NodeReqTime, BrickReqTime, BrickUseTime, tmp, &box, psize, time);
		near = far;
	}
	if(colour.w > 0.01)
	{
		float3 nc = normalize(colour.xyz);
		float3 lamb = {0.4,0.4,0.4};
		float3 lpwr = {0.3,0.3,0.3};
		float3 ldir = {0,-1,0};
		// diffuse lighting
		float ill  = dot(ldir, nc);
		colour.xyz = (float3)(lamb + ill * lpwr);
		// specular lighting
		float3 h = normalize( ldir + n );
		ill = clamp( pow( dot( nc, h ), 15.0 ), 0.0, 1.0);
		colour.xyz += (float3)(lpwr * ill);
	}
	else colour.xyz = 0;
	// debug information
//	colour.z = (float)i / 50.0;
//	colour.x = (float)ib / 15.0;
//	colour.w = 1.0;

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

__kernel void ResetBrickTime(const int time,
		__global int* BrickUseTime,
		__global int* BrickLRU)
{
	int i = get_global_id(0);
	BrickUseTime[i] = time;
	BrickLRU[i] = i;
}


__kernel void NodeLRUsort(const int time,
				__global int* NodeUseTime,
				__global int* NodeLRU,
				__global int* NodeLRUOut)
{
	__local volatile int atm_used, atm_unused;
	atm_used = 0; atm_unused = B_COUNT-1;
	barrier(CLK_LOCAL_MEM_FENCE);
	int i = NodeLRU[get_global_id(0)];
	int node_time = NodeUseTime[i];
	if(time == node_time)
	{
		NodeLRUOut[atomic_inc(&atm_used)] = i;
	}
	else
	{
		NodeLRUOut[atomic_dec(&atm_unused)] = i;
	}
}

__kernel void BrickLRUsort(const int time,
				__global int* BrickUseTime,
				__global int* BrickLRU,
				__global int* BrickLRUOut)
{
	__local volatile int atm_used, atm_unused;
	atm_used = 0; atm_unused = NP_SIZE-1;
	barrier(CLK_LOCAL_MEM_FENCE);
	int i = BrickLRU[get_global_id(0)];
	int brick_time = BrickUseTime[i];
	if(time == brick_time)
	{
		BrickLRUOut[atomic_inc(&atm_used)] = i;
	}
	else
	{
		BrickLRUOut[atomic_dec(&atm_unused)] = i;
	}
}

__kernel void NodeAlloc(const int time,
				__global int* NodeNode,
				__global int* NodeBrick,
				__global int* NodeReqTime,
				__global int* NodeLRU)
{
	__local volatile int atm;
	atm = NP_SIZE - 1;
	barrier(CLK_LOCAL_MEM_FENCE);
	int i = get_global_id(0);
	int req_time = NodeReqTime[i];
	if(time != req_time)return;

	int out = atomic_dec(&atm);
	NodeNode[i] = out;
	// now clear the newly allocated node
	for(int j=0; j<8; j++)
	{
		int offset = out * 8 + j;
		NodeNode[offset] = 0;
		NodeBrick[offset] = 0;
	}
}


__kernel void BrickAlloc(const int time,
				__write_only image3d_t bricks,
				__global int* NodeBrick,
				__global int* BrickReqTime,
				__global int* BrickLRU)
{
	__local volatile int atm;
	atm = B_COUNT - 1;
	barrier(CLK_LOCAL_MEM_FENCE);
	int i = get_global_id(0);
	int req_time = BrickReqTime[i];
	if(time != req_time)return;

	int out = atomic_dec(&atm);
	NodeBrick[i] = out;
/*
	int3 bpos = brick_origini(out);
	// now clear the newly allocated brick
	for(int x=0; x<B_SIZE; x++)
	for(int y=0; y<B_SIZE; y++)
	for(int z=0; z<B_SIZE; z++)
		write_imagef(bricks, bpos+(int3)(x,y,z), (float4)(0,0,0,0));
*/
}
