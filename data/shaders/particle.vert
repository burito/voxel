#version 410 core
uniform mat4 modelview;
uniform mat4 projection;
layout(location = 0) in vec3 inPosition;
layout(location = 3) in vec4 inColor;

layout (location = 0) out vec4 fragColor;


void main()
{
	fragColor = inColor;
	gl_Position = projection * modelview * vec4(inPosition, 1);
}

