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


bool slab_ray(in vec3 p, in vec3 inv, in vec4 box)
{
	vec3 near = (box.xyz - p) * inv;
	vec3 far = ((box.xyz+box.w) - p) * inv;
	vec3 tmin = min(near, far), tmax = max(near, far);
	float rmin = max(tmin.x, max(tmin.y, tmin.z));
	float rmax = min(tmax.x, min(tmax.y, tmax.z));
	return (rmax >= rmin) && (rmax >= 0.0);
}

float slab_exit(in vec3 p, in vec3 inv, in vec4 box)
{
	vec3 ret, nearfar[2];
	nearfar[0] = (box.xyz - p) * inv;
	nearfar[1] = ((box.xyz+box.w) - p) * inv;
	ivec3 ind = ivec3(step(0.0, inv));
	ret.x = nearfar[ind.x].x;
	ret.y = nearfar[ind.y].y;
	ret.z = nearfar[ind.z].z;
	return min(min(ret.x, ret.y), ret.z);
}

float slab_enter(in vec3 p, in vec3 inv, in vec4 box)
{
	vec3 ret, nearfar[2];
	nearfar[0] = (box.xyz - p) * inv;
	nearfar[1] = ((box.xyz+box.w) - p) * inv;
	ivec3 ind = ivec3(step(inv, vec3(0.0)));
	ret.x = nearfar[ind.x].x;
	ret.y = nearfar[ind.y].y;
	ret.z = nearfar[ind.z].z;
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



uint find_brick(in vec3 pos, inout vec4 box, in float size)
{
	box = vec4(0,0,0,1);

	uint child = 0;
	uint parent = 0;
	uint tmp;

//	while(volume > size)
	for(int i=0; i<10; i++)
	{
		child = oct_child(pos, box);
//		nPool[NP_LRU+index].n[child].child = now;
		tmp = nPool[parent].n[child].child;
		if(tmp==0)
		{
//			nPool[NP_REQ+index].n[child].child = now;
			break;
		}
		parent = tmp;
	}

	return nPool[parent].n[child].brick;
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



vec3 box_norm(in vec3 pos, in vec4 box)
{
	float wiggle = box.w * 0.0001;
	vec3 n = vec3(0);
	vec3 l = abs(box.xyz - pos);
	vec3 h = abs((box.xyz + box.w) - pos);
	if(l.x < wiggle)n.x = -1.0;
	if(l.y < wiggle)n.y = -1.0;
	if(l.z < wiggle)n.z = -1.0;
	if(h.x < wiggle)n.x = 1.0;
	if(h.y < wiggle)n.y = 1.0;
	if(h.z < wiggle)n.z = 1.0;
	return n;
}


void brick_path(in vec3 pos, in vec3 normal, in float ratio)
{
	vec4 box = vec4(0,0,0,1);
	float size = 0.0000001;

	vec3 n = normalize(normal);
	vec3 invnorm = 1.0 / n;
	float dist =0;
	vec3 normsign = sign(n);

	vec3 far, near = pos;
	if(!inside(pos, box))
	{
		if(!slab_ray(near, invnorm, box))
			discard;
		dist = slab_enter(near, invnorm, box);	// find entry point
		near += n * dist;		// advance near point
	}
	else near = pos;

	vec4 colour = vec4(0.0,0.0,0.0,0.0);
	uint brick_id = find_brick(near, box, size);
	float rem = 0.0;
	for(int i=0; i<10 && colour.w < 1.0; i++)
	{
		dist = slab_exit(near, invnorm, box);
		far = near + n * dist;

		if(brick_id != 0)
		{
			vec3 brick_near = (near - box.xyz) / box.w;
			vec3 brick_far = (far - box.xyz) / box.w;
			brick_ray(brick_id, brick_near, brick_far, colour, rem);
//			break;
		}
		else rem = 0; // box.w / float(B_SIZE);

	
		vec3 tbox = box.xyz + vec3(box.w * 0.5);
		tbox = box.xyz + (box_norm(far, box) * vec3(box.w));
		if(!inside(tbox, vec4(0,0,0,1)))break;


		vec3 tmp = far + n*0.000010;
		if(!inside(tmp, vec4(0,0,0,1)))break;
		brick_id = find_brick(tmp, box, size);


		near = far;
	}
	n = normalize(colour.xyz);

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
