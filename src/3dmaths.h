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

#ifndef __3DMATHS_H_
#define __3DMATHS_H_ 1


#ifdef WATCOM
#define sqrtf sqrt
#define sinf sin
#define cosf cos
#define strtof(x,y) atof(x)
#endif


typedef struct int2 {
	int x, y;
} int2;

typedef struct int3 {
	int x, y, z;
} int3;

typedef struct byte4 {
	unsigned char x,y,z,w;
} byte4;


float finvsqrt(float x);

typedef union {
	struct { float x, y; };
	float f[2];
} coord;

typedef union {
	struct { float x, y, z; };
	struct { coord xy; float fz; };
	struct { float fx; coord yz; };
	float f[3];
} vect;

typedef union {
	struct { float x, y, z, w; };
	struct { vect xyz; float vw; };
	float f[4];
} vec4;

typedef union {
	float f[16];
	float m[4][4];
} mat4x4;

// taken from Valve's OpenVR headers, don't use
#ifndef __OPENVR_API_FLAT_H__
typedef struct HmdMatrix34_t
{
	float m[3][4]; //float[3][4]
} HmdMatrix34_t;

typedef struct HmdMatrix44_t
{
	float m[4][4]; //float[4][4]
} HmdMatrix44_t;
#endif

void mat4x4_print(mat4x4 m);
mat4x4 mat4x4_invert(mat4x4 m);
mat4x4 mat4x4_transpose(mat4x4 m);
mat4x4 mat4x4_identity(void);
mat4x4 mat4x4_rot_x(float t);
mat4x4 mat4x4_rot_y(float t);
mat4x4 mat4x4_rot_z(float t);
mat4x4 mat4x4_translate(vect v);
mat4x4 mat4x4_translate_float(float x, float y, float z);
mat4x4 mat4x4_perspective(float near, float far, float width, float height);
mat4x4 mat4x4_orthographic(float near, float far, float width, float height);


vect vect_norm(vect v);
float vect_dot(vect l, vect r);
vect vect_cross(vect l, vect r);


/*
The following functions are to be called via the 
_Generic() macro's mag(), max(), mov(), mul(), add() and sub()
*/

float coord_mag(coord c);
float vect_mag(vect v);

float coord_max(coord c);
float vect_max(vect v);

mat4x4 mat4x4_mov_HmdMatrix34(HmdMatrix34_t x);
mat4x4 mat4x4_mov_HmdMatrix44(HmdMatrix44_t x);

mat4x4 mat4x4_mul_mat4x4(mat4x4 l, mat4x4 r);
vect mat4x4_mul_vect(mat4x4 l, vect r);
mat4x4 mat4x4_mul_float(mat4x4 l, float r);
mat4x4 mat4x4_add_mat4x4(mat4x4 l, mat4x4 r);
mat4x4 mat4x4_add_float(mat4x4 l, float r);
mat4x4 mat4x4_sub_mat4x4(mat4x4 l, mat4x4 r);

vect vect_mul_vect(vect l, vect r);
vect vect_mul_float(vect l, float r);
vect vect_div_vect(vect l, vect r);
vect vect_div_float(vect l, float r);
vect vect_add_vect(vect l, vect r);
vect vect_add_float(vect l, float r);
vect vect_sub_vect(vect l, vect r);

float float_mul(float l, float r);
float float_add(float l, float r);
float float_sub_float(float l, float r);
vect float_sub_vect(float l, vect r);
float float_div_float(float l, float r);

int int_mul(int l, int r);
int int_add(int l, int r);
int int_sub(int l, int r);
int int_div_int(int l, int r);

int2 int2_add(int2 l, int2 r);


// returns the magnitude of a vector
#define mag(X) _Generic(X, \
	coord: coord_mag, \
	vect: vect_mag \
	)(X)

// returns the largest item in a vector
#define vmax(X) _Generic(X, \
	coord: coord_max, \
	vect: vect_max \
	)(X)


#define mov(X) _Generic(X, \
	HmdMatrix34_t: mat4x4_mov_HmdMatrix34, \
	HmdMatrix44_t: mat4x4_mov_HmdMatrix44 \
	)(X)


#define mul(X,Y) _Generic(X, \
	mat4x4: _Generic(Y, \
		mat4x4: mat4x4_mul_mat4x4, \
		vect: mat4x4_mul_vect, \
		default: mat4x4_mul_float), \
	vect: _Generic(Y, \
		vect: vect_mul_vect,	\
		default: vect_mul_float), \
	float: float_mul, \
	default: int_mul \
	)(X,Y)

#define add(X,Y) _Generic(X, \
	mat4x4: _Generic(Y, \
		mat4x4: mat4x4_add_mat4x4, \
		default: mat4x4_add_float), \
	vect: _Generic(Y, \
		vect: vect_add_vect, \
		default:vect_add_float), \
	float: float_add, \
	int2: int2_add, \
	default: int_add \
	)(X,Y)

#define sub(X,Y) _Generic(X, \
	mat4x4: mat4x4_sub_mat4x4, \
	vect: vect_sub_vect,	\
	float: _Generic(Y, \
		vect: float_sub_vect, \
		default:float_sub_float), \
	default: int_sub \
	)(X,Y)

#define div(X,Y) _Generic(X, \
	vect: _Generic(Y, \
		vect: vect_div_vect,	\
		default: vect_div_float), \
	float: float_div_float, \
	default: int_div_int \
	)(X,Y)


#endif /* __3DMATHS_H_ */
