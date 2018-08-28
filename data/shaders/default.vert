#version 410 core
uniform mat4 world;
uniform mat4 camera;
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 v3NormalIn;
layout(location = 2) in vec2 v2TexCoordsIn;
out vec2 v2TexCoord;
out vec3 v3Normal;
out vec3 v3camNormal;
void main()
{
	v2TexCoord = v2TexCoordsIn;
	v3Normal = (world * vec4(v3NormalIn.xyz, 0)).xyz;
	v3camNormal = (camera * vec4(v3Normal, 0)).xyz;
	v3camNormal = vec3(v3camNormal.xy, -v3camNormal.z);
	gl_Position = camera * world * vec4(position.xyz, 1);
}

