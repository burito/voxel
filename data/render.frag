/*
Copyright (c) 2012 Daniel Burke

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

#version 410
//#extension GL_NV_gpu_program5:enable
#extension GL_NV_gpu_shader5:enable
#extension GL_NV_shader_buffer_load:enable
#extension GL_EXT_shader_image_load_store:enable
// #extension GL_EXT_texture3D:enable // part of OpenGL 4.1

#define B_SIZE 8
#define B_COUNT 48


#define NP_SIZE 100000
//#define NP_LRU NP_SIZE
//#define NP_REQ (NP_SIZE*2)


struct NodeItem {
	uint child;
	uint brick;
};

struct NodeBlock {
	NodeItem n[8];
};

//uniform int now;
uniform sampler3D bPool;
uniform NodeBlock *nPool;
uniform vec3 position;
uniform vec3 angle;

in vec3 TexCoord;
out vec4 color;


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



uint find_brick(in vec3 pos, inout vec4 box, in float size)
{
	box = vec4(0,0,0,1);

	uint child = 0;
	uint index = 0;
	uint tmp;
	float volume = 1.0/float(B_SIZE);

	while(volume > size)
	{
		volume *= 0.5;
		child = oct_child(pos, box);
//		nPool[NP_LRU+index].n[child].child = now;
		tmp = nPool[index].n[child].child;
		if(tmp==0)
		{
//			nPool[NP_REQ+index].n[child].child = now;
			break;
		}
		index = tmp;
	}

	return nPool[index].n[child].brick;
	if(tmp==0)
	{
//		nPool[NP_REQ+index].n[child].brick = now;
		return 0;
	}
//	nPool[NP_LRU+index].n[child].brick = now;
}

#define WIGGLE 0.0000001
bool inside(in vec3 pos, in vec4 box)
{
	if(pos.x >= box.x-WIGGLE && pos.x < box.x+box.w+WIGGLE)
	if(pos.y >= box.y-WIGGLE && pos.y < box.y+box.w+WIGGLE)
	if(pos.z >= box.z-WIGGLE && pos.z < box.z+box.w+WIGGLE)
		return true;
	return false;
}

// assumes ray origin is outside of brick
// returns true, and the point that it intersects
// if it doesn't intersect, returns false and leaves garbage in result
bool enter(in vec3 pos, in vec3 normal, out vec3 res)
{
	vec3 near, far;
	
	res = vec3(0);

	near.x = normal.x < 0.0 ? 1.0 : 0.0;
	near.y = normal.y < 0.0 ? 1.0 : 0.0;
	near.z = normal.z < 0.0 ? 1.0 : 0.0;
	far = vec3(1,1,1) - near;

/*
	if(normal.x < 0.0)
	{
		near.x = 1.0;
		far.x = 0.0;
		if(pos.x < far.x)
			return false;
	}
	else
	{
		near.x = 0.0;
		far.x = 1.0;
		if(pos.x > far.x)
			return false;
	}

	if(normal.y < 0.0)
	{
		near.y = 1.0;
		far.y = 0.0;
		if(pos.y < far.y)
			return false;
	}
	else
	{
		near.y = 0.0;
		far.y = 1.0;
		if(pos.y > far.y)
			return false;
	}

	if(normal.z < 0.0)
	{
		near.z = 1.0;
		far.z = 0.0;
		if(pos.z < far.z)
			return false;
	}
	else
	{
		near.z = 0.0;
		far.z = 1.0;
		if(pos.z > far.z)
			return false;
	}
*/
	vec3 rationear = (near - pos)/normal;
	vec3 ratiofar = (far - pos)/normal;
	float near_min = min(min(rationear.x, rationear.y), rationear.z);
	float near_max = max(max(rationear.x, rationear.y), rationear.z);
	float near_mid = rationear.x == near_min ?
		(rationear.y == near_min ? rationear.z : rationear.y) :
		(rationear.z == near_min ? rationear.x : rationear.z); 

	vec4 vol = vec4(-WIGGLE, -WIGGLE, -WIGGLE, 1.0+WIGGLE);

	if(inside(near_min * normal + pos, vol))
	{
		res = normal*near_min + pos;
		return true;
	}
	if(inside(near_mid * normal + pos, vol))
	{
		res = normal*near_mid + pos;
		return true;
	}
	if(inside(near_max * normal + pos, vol))
	{
		res = near_max*normal + pos;
		return true;
	}
	return false;
}


// we know we're inside the volume
vec3 escape(in vec3 pos, in vec3 normal, in vec4 box)
{
	vec3 far;
	far.x = normal.x < 0 ? box.x : box.x + box.w;
	far.y = normal.y < 0 ? box.y : box.y + box.w;
	far.z = normal.z < 0 ? box.z : box.z + box.w;

	if(normal.x == 0) normal.x += 0.00001;
	if(normal.y == 0) normal.y += 0.00001;
	if(normal.z == 0) normal.z += 0.00001;

	vec3 ratio = (far - pos)/normal;
	float rmin = min(min(ratio.x, ratio.y), ratio.z);
	return rmin * normal + pos;
}

vec3 brick_origin(in uint brick_id)
{
	vec3 brick_pos;
	uint brick_tmp;

	brick_pos.x = float(brick_id % B_COUNT);
	brick_tmp = (brick_id - uint(brick_pos.x))/B_COUNT;
	brick_pos.y = float(brick_tmp % B_COUNT);
	brick_pos.z = float((brick_tmp - uint(brick_pos.y))/B_COUNT);

	return brick_pos / float(B_COUNT);
}



void brick_ray(in uint brick_id, in vec3 near, in vec3 far, inout vec4 colour, inout float remainder)
{
//	vec3 delta = far - near;
	float iSize = 1.0 / float(B_SIZE);
	float iCount = 1.0 / float(B_COUNT);
	float hs = iSize * 0.5;
	near = near * (iSize * (B_SIZE-1)) + vec3(hs);
	far = far * (iSize * (B_SIZE-1)) + vec3(hs);
	float len = length(far - near);
	vec3 brick_pos = brick_origin(brick_id);
	vec4 result = colour;

	if(len < 0.1)return;

	float steps = 1.0/(len / iSize);

	float pos;
	for(pos=steps*remainder; pos < 1.0; pos += steps)
	{	
		vec3 sample_loc = mix(near, far, pos)*iCount;
		vec4 voxel = texture(bPool, sample_loc+brick_pos);
		result.xyz += ((1.0-result.w)*(1.0-result.w) * voxel.xyz)/(1.0 - result.xyz * voxel.xyz);
		result.w = result.w + (1.0-result.w) * voxel.w;
//		result.xyz = result.xyz + (1.0-result.w) * voxel.xyz;
//		result.w = result.w + (1.0-result.w) * voxel.w;
		if(result.w > 1.0)break;
	}
	remainder = (pos - 1.0) /steps;
	colour = result;
	// TODO: interpolate between different resolutions
}


void brick_path(in vec3 pos, in vec3 normal, in float ratio)
{
	vec4 box = vec4(0,0,0,1);
	float size = 0.0000001;
	float distance=0;
	
	vec3 hit;
	if(!inside(pos, box))
	{
		if(!enter(pos, normal, hit))
			discard;
	}
	else hit = pos;


	float len = length(hit - pos);
	distance += len;

	vec4 colour = vec4(0.0,0.0,0.0,0.0);
	uint brick_id = find_brick(hit, box, size);
	float rem = 0.0;
	for(int i=0; i<50 && colour.w < 1.0; i++)
	{
		vec3 far = escape(hit, normal, box);
		len = length(far - hit);
		vec3 step = normal * len;
		if(brick_id != 0)
		{
			vec3 brick_near = (hit - box.xyz) / box.w;
			vec3 brick_far = (far - box.xyz) / box.w;
			brick_ray(brick_id, brick_near, brick_far, colour, rem);
		}
		else rem = 0; // box.w / float(B_SIZE);

		vec3 tmp = hit + step*1.010;
//		vec3 tmp = far + normal*box.w/float(B_SIZE)*rem;
		if(!inside(tmp, vec4(0,0,0,1)))break;
		brick_id = find_brick(tmp, box, size);
		hit = tmp;
	}
	vec3 n = normalize(colour.xyz);

	vec3 lamb = vec3(0.4);
	vec3 lpwr = vec3(0.3);
	vec3 ldir = vec3(0,-1,0);
	// diffuse lighting
	float i  = dot(ldir, n);
	color = vec4(lamb+ i * lpwr, colour.w);
	// specular lighting
	vec3 h = normalize( ldir + normal );
	i = clamp( pow( dot( n, h ), 15 ), 0, 1);
	color += vec4( i * lpwr, 0);
	return;
}


void trace_ortho(void)
{
	vec3 pos = position + vec3(TexCoord.xy, 0);
	vec3 normal = vec3(0,0,1);
	brick_path(position, normal, 0);
}

void trace_fov(void)
{
	vec3 normal = normalize(TexCoord);
	brick_path(position, normal, 0.00000001);
}


void main(void)
{
//	trace_ortho();
	trace_fov();
//	gl_FragColor = vec4(0.5, 0.5, 0.5, 1);
}


/*
void shader()
{
	MakeBufferResidentNV(nodepool, READ_WRITE);
	MakeBufferResidentNV(blockpool, READ_ONLY);

	trace_ortho();

	MakeBufferNonResidentNV(nodepool);
	MakeBufferNonResidentNV(blockpool);
}

void interpass()
{
	MemoryBarrierNV();
}
*/
