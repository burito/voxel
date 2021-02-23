#version 130

uniform mat4 modelview;
uniform mat4 projection;

in vec3 inPosition;
//in vec3 inNormal;
in vec2 inUV;

out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragUV;

void main(void)
{
	gl_Position = vec4(inPosition, 1) * projection;
	fragUV = gl_MultiTexCoord0.xy;
//	fragUV = inUV;
//	fragNormal = inNormal;
	fragPosition = inPosition;
}
