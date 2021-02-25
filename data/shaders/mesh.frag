#version 410 core

uniform sampler2D diffuse;
uniform sampler2D specular;
uniform float time;


layout (location = 0) in vec2 fragUV;
layout (location = 1) in vec3 fragNormal;

layout (location = 0) out vec4 outColor;

void main()
{
	vec3 light = normalize(vec3(sin(time), cos(time), 5.));

	// test if there is a texture bound
	if( textureSize(diffuse, 0).x > 1 )
	{
		float ambient = 0.3;
		float diffuse_term = 0.4;
		float specular_term = 0.3;
		vec4 diff = texture( diffuse, fragUV);
		vec4 spec = texture( specular, fragUV);
//		outColor = vec4(diff.xyz * fragNormal.z, diff.w); // + vec4(fragNormal*0.5, 1);
		float ld = dot(fragNormal, light ) * specular_term;
		outColor = vec4( (spec.xyz*ld + diff.xyz*fragNormal.z*diffuse_term + diff.xyz*ambient), diff.w);
	}
	else
	{	// render it with normals, because normals are cool
		outColor = vec4(fragNormal, 1);
	}
}
