#version 430

layout (location = 0) in vec3 v_vertex;	// vertex coordinates in model space
layout (location = 1) in vec3 v_normal;	// vertex normal in model space

out vec3 f_vertexInView;	// calculate lighting of vertex in camera space
out vec3 f_normalInView;	// calculate lighting of normal in camera space

uniform mat4 um4p;	// projection matrix
uniform mat4 um4v;	// camera viewing transformation matrix
uniform mat4 um4n;	// model normalization matrix
uniform mat4 um4r;	// rotation matrix

// To simplify the calculation, we do calculation for lighting in camera space 
void main() {
	
	// [TODO] transform vertex and normal into camera space
	vec4 vertexInView = um4v * um4r * um4n * vec4(v_vertex, 1.0);
	vec4 normalInView = transpose(inverse(um4v * um4r * um4n)) * vec4(v_normal, 0.0);

	f_vertexInView = vertexInView.xyz;
	f_normalInView = normalInView.xyz;

	gl_Position = um4p * um4v * um4r * um4n * vec4(v_vertex, 1.0);
}
