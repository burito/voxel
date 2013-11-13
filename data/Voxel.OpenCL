
#define B_SIZE 8
#define B_COUNT 48

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

/* original slab_exit(), from 
// http://www.opengl.org/discussion_boards/showthread.php/176344-Ray-vs-AABB-exit-point-knowing-entry-point
vec3 ray_box_exit(in Ray r, in Box b)
{// compute exit point of ray from a AABB
	vec3 tmax;
	vec3 div = 1.0 / r.d;
	ivec3 indexes = ivec3(step(0.0,div));
	tmax.x = (b.m[indexes[0]].x - r.o.x) * div.x;
	tmax.y = (b.m[indexes[1]].y - r.o.y) * div.y;
	tmax.z = (b.m[indexes[2]].z - r.o.z) * div.z;
 
	return r.o + min(min(tmax.x, tmax.y), tmax.z) * r.d;
}
*/
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



__kernel void voxel(
			__write_only image2d_t out,
			__read_only image3d_t brick,
			__global float4 *camera)
//			const float time)
//			__global float* angle)
{
	int2 ipos = {get_global_id(0), get_global_id(1)};
	float2 size = {get_global_size(0), get_global_size(1)};
	float2 fpos = {ipos.x, ipos.y};
	// Translate OpenCL coordinates into per pixel normal
	fpos = (fpos / size) - 0.5;
	float3 n = normalize((float3)(fpos, 0.5));

	float4 colour = { 0, 0, 0, 1 };

//	float3 p = (camera[0], camera[1], camera[2]);
	float3 p = camera[0].xyz;
	float3 angle = camera[1].xyz;

	// rotate this normal to face the direction of the camera
//	float3 angle = ( camera[4], camera[5], camera[6] );
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

	if(!slab_ray(p, in, box))
	{
		colour = 0;
		write_imagef(out, ipos, colour);
		return;
	}

	colour = (float4)(0,1,0,1);

	if(!inside(p, box))
	{
		float dist = slab_enter(p, in, box);
		p += n * dist;
		colour = (float4)(0,1,dist*0.5,1);
		// ray dispersion should happen when p moves
	}

	write_imagef(out, ipos, colour);
}

