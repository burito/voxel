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

#include <math.h>

#include "3dmaths.h"

/* Fast Inverse Square Root
 * http://www.beyond3d.com/content/articles/8/
 */
float finvsqrt(float x)
{
    float xhalf = 0.5f*x;
    int i = *(int*)&x;
    i = 0x5f3759df - (i>>1);
    x = *(float*)&i;
    x = x*(1.5f - xhalf*x*x); /* repeat this line to improve accuracy */
    return x;
}

/*
 * Vector manipulation functions
 */

void vect_sdivide(float3 *result, const float3 *vect, const float scalar)
{
	result->x = vect->x / scalar;
	result->y = vect->y / scalar;
	result->z = vect->z / scalar;
}


void vect_madd(float3 *result, const float scalar,
		const float3 *left, const float3 *right)
{
	result->x = scalar * left->x + right->x;
	result->y = scalar * left->y + right->y;
	result->z = scalar * left->z + right->z;
}


void vect_smul(float3 *result, const float3 *left, const float right)
{
	result->x = left->x * right;
	result->y = left->y * right;
	result->z = left->z * right;
}


void vect_mul(float3 *result, const float3 *left, const float3 *right)
{
	result->x = left->x * right->x;
	result->y = left->y * right->y;
	result->z = left->z * right->z;
}


void vect_add(float3 *result, const float3 *left, const float3 *right)
{
	result->x = left->x + right->x;
	result->y = left->y + right->y;
	result->z = left->z + right->z;
}


void vect_sadd(float3 *result, const float3 *left, const float right)
{
	result->x = left->x + right;
	result->y = left->y + right;
	result->z = left->z + right;
}


void vect_sub(float3 *result, const float3 *left, const float3 *right)
{
	result->x = left->x - right->x;
	result->y = left->y - right->y;
	result->z = left->z - right->z;
}


float vect_magnitude(const float3 *vect)
{
	return vect->x * vect->x + vect->y * vect->y + vect->z * vect->z;
}


void vect_norm(float3 * result, const float3 *vect)
{
	float len = sqrt(vect_magnitude(vect));
	float scale = 1.0f / len;
	result->x = vect->x * scale;
	result->y = vect->y * scale;
	result->z = vect->z * scale;
}


/* inner product (dot product) of two vectors */
float vect_dot(const float3 *left, const float3 *right)
{
	return left->x * right->x + left->y * right->y + left->z * right->z;
}


/* outer product (cross product) of two vectors */
void vect_cross(float3 *result, const float3 *left, const float3 *right)
{
	result->x = left->y * right->z - left->z * right->y;
	result->y = left->z * right->x - left->x * right->z;
	result->z = left->x * right->y - left->y * right->x;
}

