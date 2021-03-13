#version 410 core

layout (location = 0) in vec4 inPosition;
layout (location = 1) in vec2 inUV;
layout (location = 0) noperspective out vec2 fragUV;

void main()
{
	fragUV = inUV;
	gl_Position = inPosition;
}
