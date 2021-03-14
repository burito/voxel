#version 130

uniform mat4 modelview;
uniform mat4 projection;

in vec3 inPosition;
in vec2 inUV;
in vec3 inNormal;

out vec3 fragPosition;
noperspective out vec2 fragUV;
out vec3 fragNormal;
//out vec4 vColour;

void main(void)
{
//	gl_Position = ftransform();
//	gl_Position = gl_ModelViewMatrix * gl_Vertex;
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * gl_Vertex;
//	gl_Position = projection * gl_ModelViewMatrix * gl_Vertex;
//	gl_Position = gl_ProjectionMatrix * modelview * gl_Vertex;

//	gl_Position = vec4(gl_Vertex.xyz, 1) * modelview * projection;
//	gl_Position = projection * modelview * vec4(gl_Vertex.xyz, 1);
//	gl_Position = projection * modelview * vec4(inPosition, 1);


	fragUV = inUV;
	fragNormal = inNormal;
	fragPosition = gl_Vertex.xyz;
//	fragPosition = inPosition;
//	gl_Position = projection * gl_Vertex;
//	fragPosition = gl_Position.xyz;
//	vColour = gl_Color;
}
