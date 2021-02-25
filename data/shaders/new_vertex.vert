#version 410 core

in vec4 position;
in vec2 v2UVIn;
noperspective out vec2 fragUV;

void main()
{
	fragUV = v2UVIn;
	gl_Position = position;
}
