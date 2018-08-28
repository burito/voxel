#version 410 core
uniform sampler2D diffuse;
in vec2 v2TexCoord;
in vec3 v3Normal;	// world relative normals
in vec3 v3camNormal;	// camera relative normals
out vec4 outputColor;
void main()
{
	outputColor = texture( diffuse, v2TexCoord);

//	float ld = dot(v3Normal, vec3(0,-1,0));

//	outputColor = vec4(v3camNormal, 1);
}