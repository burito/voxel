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


#include "invert4x4_sse.h"


#include <math.h>
#include <stdio.h>

#include "3dmaths.h"

/* Fast Inverse Square Root
 * http://www.beyond3d.com/content/articles/8/
 */
float finvsqrt(float x)
{
	union finv { int i; float f; } u;
	u.f = x;

	float xhalf = 0.5f*x;
	u.i = 0x5f3759df - (u.i>>1);
	x = u.f;
	x = x*(1.5f - xhalf*x*x); /* repeat this line to improve accuracy */
	return x;
}

void mat4x4_print(mat4x4 m)
{
	printf("\t%f\t%f\t%f\t%f\n\t%f\t%f\t%f\t%f\n\t%f\t%f\t%f\t%f\n\t%f\t%f\t%f\t%f\n",
		m.m[0][0], m.m[1][1], m.m[0][2], m.m[0][3],
		m.m[1][0], m.m[1][1], m.m[1][2], m.m[1][3],
		m.m[2][0], m.m[2][1], m.m[2][2], m.m[2][3],
		m.m[3][0], m.m[3][1], m.m[3][2], m.m[3][3]
	);
}

mat4x4 mat4x4_invert(mat4x4 m)
{
	mat4x4 x;
	invert4x4(m.f, x.f);
	return x;
}

mat4x4 mat4x4_transpose(mat4x4 m)
{
	mat4x4 x = {
	m.m[0][0], m.m[1][0], m.m[2][0], m.m[3][0],
	m.m[0][1], m.m[1][1], m.m[2][1], m.m[3][1],
	m.m[0][2], m.m[1][2], m.m[2][2], m.m[3][2],
	m.m[0][3], m.m[1][3], m.m[2][3], m.m[3][3]
	};
	return x;
};

mat4x4 mat4x4_identity(void)
{
	mat4x4 a = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	return a;
}

mat4x4 mat4x4_rot_x(float t)
{
	mat4x4 a = {
		1,       0,      0, 0,
		0,  cos(t), sin(t), 0,
		0, -sin(t), cos(t), 0,
		0,       0,      0, 1
	};
	return a;
}

mat4x4 mat4x4_rot_y(float t)
{
	mat4x4 a = {
		cos(t), 0, -sin(t), 0,
		     0, 1,       0, 0,
		sin(t), 0,  cos(t), 0,
		     0, 0,       0, 1
	};
	return a;
}

mat4x4 mat4x4_rot_z(float t)
{
	mat4x4 a = {
		 cos(t), sin(t), 0, 0,
		-sin(t), cos(t), 0, 0,
		      0,      0, 1, 0,
		      0,      0, 0, 1
	};
	return a;
}

mat4x4 mat4x4_translate_vect(vect v)
{
	mat4x4 a = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		v.x, v.y, v.z, 1
	};
	return a;
}

mat4x4 mat4x4_translate_float(float x, float y, float z)
{
	vect a = {x, y, z};
	return mat4x4_translate_vect(a);
}

// http://www.songho.ca/opengl/gl_projectionmatrix.html#perspective
mat4x4 mat4x4_perspective(float near, float far, float width, float height)
{
	mat4x4 ret = {
		near/(0.5*width), 0, 0, 0,
		0, near/(0.5*height), 0, 0,
		0, 0, (-(far+near))/(far-near), (-2*far*near)/(far-near),
		0, 0, -1, 0  
	};
	return ret;
}
// http://www.songho.ca/opengl/gl_projectionmatrix.html#ortho
mat4x4 mat4x4_orthographic(float near, float far, float width, float height)
{
	mat4x4 ret = {
		1/(0.5*width), 0, 0, 0,
		0, 1/(0.5*height), 0, 0,
		0, 0, (-2)/(far-near), -((far*near)/(far-near)),
		0, 0, 0, 1  
	};
	return ret;
}


float coord_mag(coord c)
{
	return c.x*c.x + c.y*c.y;
}

float vect_mag(vect v)
{
	return v.x*v.x + v.y*v.y + v.z*v.z;
}

float coord_max(coord c)
{
	return c.x >= c.y ? c.x : c.y;
}

float vect_max(vect v)
{
	return (v.x>=v.y && v.x>=v.z) ? v.x : (v.y>=v.z?v.y:v.z);
}


vect vect_norm(vect v)
{
	vect x;
	float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	float scale = 1.0f / len;
	return mul(v,scale);
}

/* inner product (dot product) of two vectors */
float vect_dot(vect l, vect r)
{
	return l.x * r.x + l.y * r.y + l.z * r.z;
}

/* outer product (cross product) of two vectors */
vect vect_cross(vect l, vect r)
{
	vect x;
	x.x = l.y * r.z - l.z * r.y;
	x.y = l.z * r.x - l.x * r.z;
	x.z = l.x * r.y - l.y * r.x;
	return x;
}

/*
The following functions are to be called via the 
_Generic() macro's mul(), add() and sub()
*/

mat4x4 mat4x4_mov_HmdMatrix34(HmdMatrix34_t x)
{
	mat4x4 r = {
		x.m[0][0], x.m[1][0], x.m[2][0], 0.0f,	
		x.m[0][1], x.m[1][1], x.m[2][1], 0.0f,	
		x.m[0][2], x.m[1][2], x.m[2][2], 0.0f,	
		x.m[0][3], x.m[1][3], x.m[2][3], 1.0f,	
	};
	return r;
}

mat4x4 mat4x4_mov_HmdMatrix44(HmdMatrix44_t x)
{			// they're the same, trust me!
	return mat4x4_transpose( *((mat4x4*)((void*)&x)) );
}

mat4x4 mat4x4_mul_mat4x4(mat4x4 l, mat4x4 r)
{
	mat4x4 ret;
	for(int y=0; y<4; y++)
	for(int x=0; x<4; x++)
		ret.m[y][x] =
			r.m[y][0] * l.m[0][x] +
			r.m[y][1] * l.m[1][x] +
			r.m[y][2] * l.m[2][x] +
			r.m[y][3] * l.m[3][x];
	return ret;
}

vect mat4x4_mul_vect(mat4x4 l, vect r)
{
	vect x;
	for (int i = 0; i < 3; i++)
	{
		x.f[i] = l.m[i][0]*r.x + l.m[i][1]*r.y + l.m[i][2]*r.z + l.m[i][3];
	}
	return x;
}

mat4x4 mat4x4_add_mat4x4(mat4x4 l, mat4x4 r)
{
	mat4x4 x;
	for(int i=0; i<16; i++)
		x.f[i] = l.f[i] + r.f[i];
	return x;
}

mat4x4 mat4x4_add_float(mat4x4 l, float r)
{
	mat4x4 x;
	for(int i=0; i<16; i++)
		x.f[i] = l.f[i] + r;
	return x;
}

mat4x4 mat4x4_mul_float(mat4x4 l, float r)
{
	mat4x4 x;
	for(int i=0; i<16; i++)
		x.f[i] = l.f[i] * r;
	return x;
}


mat4x4 mat4x4_sub_mat4x4(mat4x4 l, mat4x4 r)
{
	mat4x4 x;
	for(int i=0; i<16; i++)
		x.f[i] = l.f[i] - r.f[i];
	return x;
}


vect vect_mul_vect(vect l, vect r)
{
	vect x;
	x.x = l.x * r.x;
	x.y = l.y * r.y;
	x.z = l.z * r.z;
	return x;
}

vect vect_div_vect(vect l, vect r)
{
	vect x;
	x.x = l.x / r.x;
	x.y = l.y / r.y;
	x.z = l.z / r.z;
	return x;
}

vect vect_add_vect(vect l, vect r)
{
	vect x = { l.x + r.x, l.y + r.y, l.z + r.z };
	return x;
}

vect vect_add_float(vect l, float r)
{
	vect x = { l.x + r, l.y + r, l.z + r };
	return x;
}

vect vect_sub_vect(vect l, vect r)
{
	vect x = { l.x - r.x, l.y - r.y, l.z - r.z };
	return x;
}

vect vect_mul_float(vect l, float r)
{
	vect a = {l.x*r, l.y*r, l.z*r};
	return a;
}

vect vect_div_float(vect l, float r)
{
	vect a = {l.x/r, l.y/r, l.z/r};
	return a;
}


int int_mul(int l, int r)
{
	return l * r;
}

int int_add(int l, int r)
{
	return l + r;
}

int int_sub(int l, int r)
{
	return l - r;
}

int int_div(int l, int r)
{
	return l / r;
}


int2 int2_add(int2 l, int2 r)
{
	int2 a = {l.x+r.x, l.y+r.y};
	return a;
}


float float_mul(float l, float r)
{
	return l * r;
}

float float_add(float l, float r)
{
	return l + r;
}

float float_sub_float(float l, float r)
{
	return l - r;
}

float float_div_float(float l, float r)
{
	return l / r;
}

vect float_sub_vect(float l, vect r)
{
	vect x = { l - r.x, l - r.y, l - r.z};
	return x;
}

