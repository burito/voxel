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

//#version 400

uniform vec3 angle;
varying out vec3 TexCoord;

void main(void)
{
	gl_Position = ftransform();
	gl_FrontColor = gl_Color;
	vec3 n = vec3(0.5 - gl_MultiTexCoord0.xy, 0.5);
	vec3 t;

	vec3 c = vec3(cos(angle.x), cos(angle.y), cos(angle.z));
	vec3 s = vec3(sin(angle.x), sin(angle.y), sin(angle.z));

	t.x = n.x * c.z + n.y * s.z;	// around z
	t.y = n.x * s.z - n.y * c.z;

	n.y = t.y * c.x + n.z * s.x;	// around x
	t.z = t.y * s.x - n.z * c.x;

	n.x = t.x * c.y + t.z * s.y;	// around y
	n.z = t.x * s.y - t.z * c.y;

	TexCoord = n;

}

