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

typedef struct float2
{
	float x;
	float y;
} float2;

typedef struct float3 {
	float x;
	float y;
	float z;
} float3;

typedef struct float4 {
	float x;
	float y;
	float z;
	float w;
} float4;

typedef struct int2 {
	int x, y;
} int2;

typedef struct int3 {
	int x, y, z;
} int3;

typedef struct byte4 {
	unsigned char x,y,z,w;
} byte4;

#define F3COPY(D,S) D.x=S.x;D.y=S.y;D.z=S.z
#define F3ADD(D,A,B) D.x=A.x+B.x;D.y=A.y+B.y;D.z=A.z+B.z
#define F3ADDS(D,A,B) D.x=A.x+B;D.y=A.y+B;D.z=A.z+B
#define F3SUB(D,A,B) D.x=A.x-B.x;D.y=A.y-B.y;D.z=A.z-B.z
#define F3MUL(D,A,B) D.x=A.x*B.x;D.y=A.y*B.y;D.z=A.z*B.z
#define F3MULS(D,A,B) D.x=A.x*B;D.y=A.y*B;D.z=A.z*B
#define F3DIV(D,A,B) D.x=A.x/B.x;D.y=A.y/B.y;D.z=A.z/B.z
#define F3MAX(D) ((D.x>=D.y && D.x>=D.z)?D.x:(D.y>=D.z?D.y:D.z))

#define F4COPY(D,S) D.x=S.x;D.y=S.y;D.z=S.z;D.w=S.w

float finvsqrt(float x);
void vect_sdivide(float3 *result, const float3 *vect, const float scalar);
void vect_madd(float3 *result, const float scalar,
		const float3 *left, const float3 *right);
void vect_smul(float3 *result, const float3 *left, const float right);
void vect_mul(float3 *result, const float3 *left, const float3 *right);
void vect_add(float3 *result, const float3 *left, const float3 *right);
void vect_sadd(float3 *result, const float3 *left, const float right);
void vect_sub(float3 *result, const float3 *left, const float3 *right);
float vect_length(const float3 *vect);
void vect_norm(float3 *result, const float3 *vect);
float vect_dot(const float3 *left, const float3 *right);
void vect_cross(float3 *result, const float3 *left, const float3 *right);

void quat_add(float4* result, float4* a, float4 *b);
void quat_mul(float4* result, float4* a, float4* b);
void quat_smul(float4* result, float4* q, float s);
float quat_mag(float4* q);
int quat_norm(float4* q);
void quat_rotx(float4* q, float x);
//void quat_rot(float3 *result, quat *a, float3 *b);
void quat_rot(float3 *result, float3 *a, float4 *b);
void quat_nlerp(float4* result, float4* left, float4* right, float t);
float quat_dot(float4 *left, float4 *right);
void quat_slerp(float4* result, float4* left, float4* right, float t);

#endif /* __3DMATHS_H_ */
