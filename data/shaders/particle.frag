#version 410 core
uniform sampler2D diffuse;

layout (location = 0) in vec4 fragColor;

layout (location = 0) out vec4 outColor;

void main()
{
	outColor = fragColor;
}