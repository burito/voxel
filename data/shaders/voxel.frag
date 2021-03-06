#version 430
#extension GL_ARB_shading_language_include : require
#include "/voxel_data"
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

precision highp float;
precision highp int;

#define B_COUNT (B_EDGE*B_EDGE*B_EDGE)

uniform mat4 modelview;
uniform mat4 projection;

uniform sampler3D bricks;
uniform sampler3D brick_col;
layout(shared, binding=0) buffer camera { vec4 f[]; } cam;
layout(shared, binding=1) buffer node_node { int i[][8]; } NodeNode;
layout(shared, binding=2) buffer node_brick { int i[][8]; } NodeBrick;
layout(shared, binding=3) buffer node_usetime { int i[]; } NodeUseTime;
layout(shared, binding=4) buffer node_reqtime { int i[][8]; } NodeReqTime;
layout(shared, binding=5) buffer brick_reqtime { int i[][8]; } BrickReqTime;
layout(shared, binding=6) buffer brick_usetime { int i[]; } BrickUseTime;

uniform int time;
uniform float screen_aspect_ratio;

layout (location = 0) noperspective in vec2 fragUV;
out vec4 Colour;



/**
 * inside - determines if a point is inside a box
 * @pos: the point in question
 * @box: the box in question
 *
 * WIGGLE is there to bais it towards the current box
 */
#define WIGGLE 0.0000001
bool inside(in vec3 pos, in vec4 box)
{
	if(pos.x >= box.x-WIGGLE && pos.x < box.x+box.w+WIGGLE)
	if(pos.y >= box.y-WIGGLE && pos.y < box.y+box.w+WIGGLE)
	if(pos.z >= box.z-WIGGLE && pos.z < box.z+box.w+WIGGLE)
		return true;
	return false;
}
#undef WIGGLE

/**
 * slab_ray - determine if a ray intersects a box
 * @p: origin of the ray
 * @inv: inverse of the ray normal
 * @box: the box in question
 *
 * Returns True on hit, and False on miss
 */
bool slab_ray(in vec3 p, in vec3 inv, in vec4 box)
{
	vec3 near = (box.xyz - p) * inv;
	vec3 far = (box.xyz+box.w - p) * inv;
	vec3 tmin = min(near, far), tmax = max(near, far);
	float rmin = max(tmin.x, max(tmin.y, tmin.z));
	float rmax = min(tmax.x, min(tmax.y, tmax.z));
	return (rmax > rmin) && (rmax >= 0);
}

/**
 * slab_exitside - determine which face a ray exits a cube
 * @p: origin of the ray
 * @inv: inverse of the ray normal
 * @box: the box in question
 *
 * Assumes the ray origin is inside the box.
 */
float slab_exitside(in vec3 p, in vec3 inv, in vec4 box, out vec3 axis)
{
	vec3 ret, nearfar[2];
	nearfar[0] = (box.xyz - p) * inv;
	nearfar[1] = (box.xyz+box.w - p) * inv;
	ivec3 ind = ivec3(step(0.0, inv));
	ret.x = nearfar[ind.x].x;
	ret.y = nearfar[ind.y].y;
	ret.z = nearfar[ind.z].z;
	ind = ind * 2 - 1;
	float r;
	if(ret.x < ret.y)
	{
		r = ret.x;
		axis = vec3(float(ind.x),0,0);	// x axis
	}
	else
	{
		r = ret.y;
		axis = vec3(0,float(ind.y),0);	// y axis
	}
	if( ret.z < r)
	{
		r = ret.z;
		axis = vec3(0,0,float(ind.z));	// z axis
	}
	return r;
}

/**
 * slab_exit - find the point a ray leaves a box
 * @p: origin of the ray
 * @inv: inverse of the ray normal
 * @box: the box in question
 *
 * Assumes the ray *will* intersect the box.
 */
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

/**
 * slab_enter - find the point a ray enters a box
 * @p: origin of the ray
 * @inv: inverse of the ray normal
 * @box: the box in question
 *
 * Assumes the ray *will* intersect the box.
 */
float slab_enter(in vec3 p, in vec3 inv, in vec4 box)
{
	vec3 ret, nearfar[2];
	nearfar[0] = (box.xyz - p) * inv;
	nearfar[1] = ((box.xyz+box.w) - p) * inv;
	ivec3 ind = ivec3(step(0.0, -inv));
	ret.x = nearfar[ind.x].x;
	ret.y = nearfar[ind.y].y;
	ret.z = nearfar[ind.z].z;
	return max(max(ret.x, ret.y), ret.z);
}


/**
 * oct_child - find which octant of a cube a point is in
 * @pos: origin of the ray
 * @vol: the box in question
 *
 * Returns the binary representation of the octant a point is in.
 * Modifies the volume to cover that octant.
 */
int oct_child(in vec3 pos, inout vec4 vol)
{
	vol.w *= 0.5;
	vec3 s = step(vol.xyz + vol.w, pos);
	vol.xyz += s * vol.w;
	s *= vec3(1,2,4);
	return int(s.x + s.y + s.z);
}

/**
 * brick_origin - get the UVW coord
 * @brick_id: the id number of a brick
 *
 * Returns the UVW coords of a brick.
 */
vec3 brick_origin(in int brick_id)
{
	vec3 brick_pos;
	int brick_tmp;

	brick_pos.x = float(brick_id % B_EDGE);
	brick_tmp = (brick_id - int(brick_pos.x))/B_EDGE;
	brick_pos.y = float(brick_tmp % B_EDGE);
	brick_pos.z = float((brick_tmp - int(brick_pos.y))/B_EDGE);

	return brick_pos / float(B_EDGE);
}


int find_brick(in vec3 pos, inout vec4 box, in float size)
{
	box = vec4(0,0,0,1);

	int child = oct_child(pos, box);
	int parent = 0;
	int tmp = 0;
	float volume = size * float(B_SIZE);
//	int last_brick = 0;

	while(volume < box.w)
	{
//		last_brick = NodeBrick.i[parent][child];
		NodeUseTime.i[parent] = time;
		tmp = NodeNode.i[parent][child];
		if(0==tmp)
		{
			NodeReqTime.i[parent][child] = time;
			break;
		}
		if(tmp < 0)return 0;
		parent = tmp;
		child = oct_child(pos, box);
	}

	tmp = NodeBrick.i[parent][child];
	if(0==tmp)
	{
		BrickReqTime.i[parent][child] = time;
	}
	else
	{
		BrickUseTime.i[tmp] = time;
	}
	return tmp;
}

void brick_ray(in int brick_id, in vec3 near, in vec3 far, inout vec4 colour,
inout vec4 normal, inout float remainder)
{
	float iSize = 1.0 / float(B_SIZE);
	float iCount = 1.0 / float(B_EDGE);
	float hs = iSize * 0.5;
	near = near * (iSize * (B_SIZE-1)) + vec3(hs);
	far = far * (iSize * (B_SIZE-1)) + vec3(hs);
	float len = length(far - near);
	vec3 brick_pos = brick_origin(brick_id);
	vec4 retnorm = normal;
	vec4 retcol = colour;

	if(len < 0.1)return;

	float steps = 0.5/(len / iSize);

	float pos;
	for(pos=steps*remainder; pos < 1.0; pos += steps)
	{
		vec3 sample_loc = mix(near, far, pos)*iCount;
		vec4 voxeln = texture(bricks, sample_loc+brick_pos);
		vec4 voxelc = texture(brick_col, sample_loc+brick_pos);
		retnorm.xyz += ((1.0-retcol.w)*(1.0-retcol.w) * voxeln.xyz)/(1.0 - retnorm.xyz * voxeln.xyz);
		retcol.xyz += ((1.0-retcol.w)*(1.0-retcol.w) * voxelc.xyz)/(1.0 - retcol.xyz * voxelc.xyz);
		retnorm.w = retnorm.w + (1.0-retnorm.w) * voxeln.w;
		retcol.w = retcol.w + (1.0-retcol.w) * voxelc.w;
		if(retcol.w > 1.0)break;
	}
	remainder = (pos - 1.0) /steps;
	normal = retnorm;
	colour = retcol;
	// TODO: interpolate between different resolutions
}

mat2 rot(float a)
{
	float c = cos(a);
	float s = sin(a);
	return mat2( vec2(c,s), vec2(-s,c) );
}

void main(void)
{
	float difference = cam.f[0].w;	// half the width of a pixel
	float fov = cam.f[1].w;
	float prat = sin(fov * (difference*2));	// pixel ratio
/*
	// use the modelview matrix to figure out where the camera is
	vec3 ptmp = ( inverse(modelview) * vec4(0.,0.,0.,1.)).xyz;
	vec3 p = ptmp;

	// and use the same, but on ray-normals, to do the projection
	vec2 tmpUV = fragUV;
	tmpUV = tmpUV * 2.0 - 1.0;
	tmpUV.y = tmpUV.y * screen_aspect_ratio;
	vec3 ntmp = (inverse(modelview) * vec4(tmpUV.x, tmpUV.y,-1., 1.)).xyz;
	vec3 rd = ntmp - ptmp;
	vec3 n = normalize(rd);
*/
	vec4 ro = inverse(modelview) * vec4(0,0,0,1);
	vec4 rd = inverse(projection ) * vec4(fragUV*2.-1., 1, 1);;
	rd.xyz = inverse(mat3(modelview)) * rd.xyz;
	vec3 n = normalize( rd.xyz );
	vec3 p = ro.xyz;
	vec3 invnorm = 1.0 / n;
	vec3 normsign = sign(n);

	// Are we inside the Octtree?
	vec4 box = vec4(0,0,0,1);
	float dist = 0;
	float psize = 0;
	vec3 far, near = p;
	if(!inside(near, box))		// if we're not inside the block
	{
		if(!slab_ray(near, invnorm, box))	// will we enter the block?
		{
//			Colour = vec4(0,1,0,1);		// not today
			discard;
			return;
		}
		dist = slab_enter(near, invnorm, box);	// find entry point
		psize = dist * prat;	// increase pixel volume
		near += n * dist;		// advance near point
	}
	float rem=0.0;

	float cy=0;
	vec4 colour = vec4(0);
	vec4 normal = vec4(0);
	int brick_id = find_brick(near, box, psize);
	int i, ib=0;
	// march through the volume until we have hit enough voxels
	for(i=0; i<80 && colour.w < 1.0; i++)
	{
		vec3 axis;
		dist = slab_exitside(near, invnorm, box, axis);
		far = near + dist * n;
		psize += dist * prat;	// increase pixel volume
		if(brick_id != 0)
		{
			vec3 brick_near = (near - box.xyz) / box.w;
			vec3 brick_far = (far - box.xyz) / box.w;
			brick_ray(brick_id, brick_near, brick_far, colour, normal, rem);
			ib++;
		}
		else rem = 0;

//		axis = axis * sign(invnorm);
//		vec3 tmp = far + axis*(box.w);
		vec3 tmp = far + axis*(box.w*0.1);
//		vec3 tmp = box.xyz + box.w*0.5 + axis*box.w;
		if(!inside(tmp, vec4(0,0,0,1)))break;
		brick_id = find_brick(tmp, box, psize);
		near = far;
	}

	// if we hit enough voxels
	if(colour.w > 0.01)
	{
		vec3 nc = normalize(normal.xyz);
//		colour.rgb = mat3(modelview) * nc;

//		float ratio = 1.0 / length(normal.zyx);
//		colour = colour *ratio;
		vec3 lamb = vec3(0.7);
		vec3 lpwr = vec3(0.3);
		vec3 ldir = vec3(0,-1,0);
		// diffuse lighting
		float ill  = dot(ldir, nc);
		colour.rgb *= vec3(lamb+ ill * lpwr);
		// specular lighting
		vec3 h = normalize( ldir + n );
		ill = clamp( pow( dot( nc, h ), 15 ), 0, 1);
		colour.rgb += vec3( ill * lpwr);

	}
	else // otherwise background colour
	{
		colour = vec4(vec3(0.0), 0.3);
//		colour = vec4(0);
	}
	// debug information
//	colour.b += float(i) / 50.0;
//	colour.r = float(ib) / 15.0;
//	colour.a = 1;

	Colour = colour; //abs(normal);
	gl_FragDepth = far.z;
	return;
}
